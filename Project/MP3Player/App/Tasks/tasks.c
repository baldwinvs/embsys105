/************************************************************************************

Copyright (c) 2001-2016  University of Washington Extension.

Module Name:

    tasks.c

Module Description:

    The tasks that are executed by the test application.

2016/2 Nick Strathy adapted it for NUCLEO-F401RE

************************************************************************************/
#include "tasks.h"

#include <stdarg.h>

#include "bsp.h"
#include "print.h"
#include "mp3Util.h"
#include "InputCommands.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>

#ifndef BUFSIZE
#define BUFSIZE 256
#endif

/************************************************************************************

Allocate the stacks for each task.
The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h

************************************************************************************/

static OS_STK TouchPollingTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK StreamingTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK LcdHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK CommandHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK TaskRxFlagsStk[APP_CFG_TASK_START_STK_SIZE];

// Useful functions
void PrintToLcdWithBuf(char *buf, int size, char *format, ...);

// Globals
BOOLEAN nextSong = OS_FALSE;

OS_FLAG_GRP *rxFlags = 0;       // Event flags for synchronizing mailbox messages
OS_EVENT * touch2CmdHandler;

INPUT_COMMAND commandPressed[1];

/************************************************************************************

This task is the initial task running, started by main(). It starts
the system tick timer and creates all the other tasks. Then it deletes itself.

************************************************************************************/
void StartupTask(void* pdata)
{
	char buf[BUFSIZE];
    uint8_t err = 0;

    touch2CmdHandler = OSMboxCreate((void*)0);
    if(NULL == touch2CmdHandler) {
        PrintWithBuf(buf, BUFSIZE, "StartupTask: failed to create touch2CmdHandler\n");
    }

    rxFlags = OSFlagCreate((OS_FLAGS)0, &err);
    if(err != OS_ERR_NONE) {
        PrintWithBuf(buf, BUFSIZE, "StartupTask: failed to create rxFlags with error %d\n", (INT32U)err);
    }

    PjdfErrCode pjdfErr;
    INT32U length;
    static HANDLE hSD = 0;
    static HANDLE hSPI = 0;

	PrintWithBuf(buf, BUFSIZE, "StartupTask: Begin\n");
	PrintWithBuf(buf, BUFSIZE, "StartupTask: Starting timer tick\n");

    // Start the system tick
    OS_CPU_SysTickInit(OS_TICKS_PER_SEC);

    // Initialize SD card
    PrintWithBuf(buf, PRINTBUFMAX, "Opening handle to SD driver: %s\n", PJDF_DEVICE_ID_SD_ADAFRUIT);
    hSD = Open(PJDF_DEVICE_ID_SD_ADAFRUIT, 0);
    if (!PJDF_IS_VALID_HANDLE(hSD)) while(1);


    PrintWithBuf(buf, PRINTBUFMAX, "Opening SD SPI driver: %s\n", SD_SPI_DEVICE_ID);
    // We talk to the SD controller over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the SD driver.
    hSPI = Open(SD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hSD, PJDF_CTRL_SD_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);

    // Create the tasks
    PrintWithBuf(buf, BUFSIZE, "StartupTask: Creating the application tasks\n");

    // The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h
//    OSTaskCreate(StreamingTask, (void*)0, &StreamingTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_STREAM_PRIO);
    OSTaskCreate(TouchPollingTask, (void*)0, &TouchPollingTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_TOUCH_PRIO);
    OSTaskCreate(CommandHandlerTask, (void*)0, &CommandHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_COMMAND_PRIO);
    OSTaskCreate(LcdHandlerTask, (void*)0, &LcdHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_LCD_PRIO);
    //Support tasks
    OSTaskCreate(TaskRxFlags, (void*)0, &TaskRxFlagsStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_RXFLAGS_PRIO);

    // Delete ourselves, letting the work be done in the new tasks.
    PrintWithBuf(buf, BUFSIZE, "StartupTask: deleting self\n");
	OSTaskDel(OS_PRIO_SELF);
}



/************************************************************************************

TaskRxFlags synchronizes TaskMBRxA and TaskMBRxB so they each must receive their current message
before either can receive their next message.

************************************************************************************/

void TaskRxFlags(void* pData)
{
	INT8U err;
	char buf[BUFSIZE];

	PrintWithBuf(buf, BUFSIZE, "TaskRxFlags: starting\n");

    while(1) {
        OSFlagPend(rxFlags, 0x1, OS_FLAG_WAIT_SET_ALL, 0, &err);
        if(OS_ERR_NONE != err) {
            PrintWithBuf(buf, BUFSIZE, "TaskRxFlags: pending on 0x1 for flag group rxFlags, err %d\n", err);
        }

        OSFlagPost(rxFlags, 0x1, OS_FLAG_CLR, &err);
        if(OS_ERR_NONE != err) {
            PrintWithBuf(buf, BUFSIZE, "ERR TaskRxFlags: posting to 0x%x for flag group rxFlags, err %d\n", 0x3, err);
        }
    }
}