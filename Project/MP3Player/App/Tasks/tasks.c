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
#include "PlayerControl.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>

/************************************************************************************

Allocate the stacks for each task.
The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h

************************************************************************************/

static OS_STK TouchPollingTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK StreamingTaskStk[APP_CFG_TASK_STK_SIZE];
static OS_STK LcdHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK CommandHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK TaskRxFlagsStk[APP_CFG_TASK_START_STK_SIZE];

// Useful functions
void PrintToLcdWithBuf(char *buf, int size, char *format, ...);

// Globals
OS_FLAG_GRP *rxFlags = 0;       // Event flags for synchronizing mailbox messages
OS_EVENT * touch2CmdHandler;
OS_EVENT * semPrint;

void* commandBuffer[4] = {0};
OS_EVENT * commandMsgQ;

INPUT_COMMAND commandPressed[1];

/************************************************************************************

This task is the initial task running, started by main(). It starts
the system tick timer and creates all the other tasks. Then it deletes itself.

************************************************************************************/
void StartupTask(void* pdata)
{
    uint8_t err = 0;

    touch2CmdHandler = OSMboxCreate((void*)0);
    if(NULL == touch2CmdHandler) {
        PrintFormattedString("StartupTask: failed to create touch2CmdHandler\n");
    }

    rxFlags = OSFlagCreate((OS_FLAGS)0, &err);
    if(err != OS_ERR_NONE) {
        PrintFormattedString("StartupTask: failed to create rxFlags with error %d\n", (INT32U)err);
    }

    semPrint = OSSemCreate(1);
    if(NULL == semPrint) {
        PrintFormattedString("StartupTask: failed to create semPrint\n");
    }

    const uint16_t commandBufferSize = sizeof(commandBuffer)/sizeof(void*);
    commandMsgQ = OSQCreate(commandBuffer, commandBufferSize);
    if(NULL == commandMsgQ) {
        PrintFormattedString("StartupTask: failed to create commandMsgQ\n");
    }

	PrintFormattedString("StartupTask: Begin\n");
	PrintFormattedString("StartupTask: Starting timer tick\n");

    // Start the system tick
    OS_CPU_SysTickInit(OS_TICKS_PER_SEC);

    // Create the tasks
    PrintFormattedString("StartupTask: Creating the application tasks\n");

    // The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h
    OSTaskCreate(StreamingTask, (void*)0, &StreamingTaskStk[APP_CFG_TASK_STK_SIZE-1], APP_TASK_STREAM_PRIO);
    OSTaskCreate(TouchPollingTask, (void*)0, &TouchPollingTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_TOUCH_PRIO);
    OSTaskCreate(CommandHandlerTask, (void*)0, &CommandHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_COMMAND_PRIO);
    OSTaskCreate(LcdHandlerTask, (void*)0, &LcdHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_LCD_PRIO);
    //Support tasks
    OSTaskCreate(TaskRxFlags, (void*)0, &TaskRxFlagsStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_RXFLAGS_PRIO);

    // Delete ourselves, letting the work be done in the new tasks.
    PrintFormattedString("StartupTask: deleting self\n");
	OSTaskDel(OS_PRIO_SELF);
}



/************************************************************************************

TaskRxFlags synchronizes TaskMBRxA and TaskMBRxB so they each must receive their current message
before either can receive their next message.

************************************************************************************/

void TaskRxFlags(void* pData)
{
	INT8U err;

	PrintFormattedString("TaskRxFlags: starting\n");

    OSFlagPend(rxFlags, 0xF, OS_FLAG_WAIT_SET_ALL, 0, &err);
    if(OS_ERR_NONE != err) {
        PrintFormattedString("TaskRxFlags: pending on 0xF for flag group rxFlags, err %d\n", err);
    }

    //TODO: Change this to maybe sending the LCD controller a message to say initialized
    PrintFormattedString("All tasks initialized!\n");

	OSTaskDel(OS_PRIO_SELF);
}