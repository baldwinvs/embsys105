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
static OS_STK LcdHandlerTaskStk[APP_CFG_TASK_STK_SIZE];
static OS_STK CommandHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK TaskRxFlagsStk[APP_CFG_TASK_START_STK_SIZE];

// Useful functions
void PrintToLcdWithBuf(char *buf, int size, char *format, ...);

// Globals
OS_FLAG_GRP *rxFlags = 0;           // Event flags for synchronizing mailbox messages
OS_EVENT * touch2CmdHandler;        // Message mailbox connecting touchPolling to commandHandler
OS_EVENT * touch2LcdHandler;        // Message mailbox connecting touchPolling to lcdHandler
OS_EVENT * cmdHandler2Stream;       // Message mailbox connecting commandHandler to streaming
OS_EVENT * cmdHandler2LcdHandler;   // Message mailbox connecting commandHandler to lcdHandler
OS_EVENT * stream2LcdHandler;       // Message mailbox connecting streaming to lcdHandler
OS_EVENT * progressMessage;         // Message mailbox used for sending progress information from streaming to lcdHandler
OS_EVENT * semPrint;

INPUT_COMMAND commandPressed[1];
uint16_t touch2LcdMessage[1];
CONTROL stateAndControl[1];
char songTitle[SONGLEN];
float progressValue = 0;

/************************************************************************************

This task is the initial task running, started by main(). It starts
the system tick timer and creates all the other tasks. Then it deletes itself.

************************************************************************************/
void StartupTask(void* pdata)
{
    uint8_t err = 0;

    touch2CmdHandler = OSMboxCreate(NULL);
    if(NULL == touch2CmdHandler) {
#if DEBUG
        PrintFormattedString("StartupTask: failed to create touch2CmdHandler\n");
#endif
    }

    touch2LcdHandler = OSMboxCreate(NULL);
    if(NULL == touch2LcdHandler) {
#if DEBUG
        PrintFormattedString("StartupTask: failed to create touch2LcdHandler\n");
#endif
    }

    cmdHandler2Stream = OSMboxCreate(NULL);
    if(NULL == cmdHandler2Stream) {
#if DEBUG
        PrintFormattedString("StartupTask: failed to create cmdHandler2Stream\n");
#endif
    }

    cmdHandler2LcdHandler = OSMboxCreate(NULL);
    if(NULL == cmdHandler2LcdHandler) {
#if DEBUG
        PrintFormattedString("StartupTask: failed to create cmdHandler2LcdHandler\n");
#endif
    }

    stream2LcdHandler = OSMboxCreate(NULL);
    if(NULL == stream2LcdHandler) {
#if DEBUG
        PrintFormattedString("StartupTask: failed to create stream2LcdHandler\n");
#endif
    }

    progressMessage = OSMboxCreate(NULL);
    if(NULL == stream2LcdHandler) {
#if DEBUG
        PrintFormattedString("StartupTask: failed to create progressMessage\n");
#endif
    }

    rxFlags = OSFlagCreate((OS_FLAGS)0, &err);
    if(err != OS_ERR_NONE) {
#if DEBUG
        PrintFormattedString("StartupTask: failed to create rxFlags with error %d\n", (INT32U)err);
#endif
    }

    semPrint = OSSemCreate(1);
    if(NULL == semPrint) {
#if DEBUG
        PrintFormattedString("StartupTask: failed to create semPrint\n");
#endif
    }

#if DEBUG
	PrintFormattedString("StartupTask: Begin\n");
	PrintFormattedString("StartupTask: Starting timer tick\n");
#endif

    // Start the system tick
    OS_CPU_SysTickInit(OS_TICKS_PER_SEC);

    // Create the tasks
#if DEBUG
    PrintFormattedString("StartupTask: Creating the application tasks\n");
#endif

    // The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h
    OSTaskCreate(StreamingTask, (void*)0, &StreamingTaskStk[APP_CFG_TASK_STK_SIZE-1], APP_TASK_STREAM_PRIO);
    OSTaskCreate(TouchPollingTask, (void*)0, &TouchPollingTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_TOUCH_PRIO);
    OSTaskCreate(CommandHandlerTask, (void*)0, &CommandHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_COMMAND_PRIO);
    OSTaskCreate(LcdHandlerTask, (void*)0, &LcdHandlerTaskStk[APP_CFG_TASK_STK_SIZE-1], APP_TASK_LCD_PRIO);
    //Support tasks
    OSTaskCreate(TaskRxFlags, (void*)0, &TaskRxFlagsStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_RXFLAGS_PRIO);

    // Delete ourselves, letting the work be done in the new tasks.
	OSTaskDel(OS_PRIO_SELF);
}



/************************************************************************************

TaskRxFlags synchronizes TaskMBRxA and TaskMBRxB so they each must receive their current message
before either can receive their next message.

************************************************************************************/

void TaskRxFlags(void* pData)
{
	INT8U err;

#if DEBUG
	PrintFormattedString("TaskRxFlags: starting\n");
#endif

    OSFlagPend(rxFlags, 0xF, OS_FLAG_WAIT_SET_ALL, 0, &err);
    if(OS_ERR_NONE != err) {
        PrintFormattedString("TaskRxFlags: pending on 0xF for flag group rxFlags, err %d\n", err);
    }

    OSFlagPost(rxFlags, 0xF, OS_FLAG_CLR, &err);
    if(OS_ERR_NONE != err) {
        PrintFormattedString("TaskRxFlags: posting 0x8 to flag group rxFlags, err %d\n", err);
    }

	OSTaskDel(OS_PRIO_SELF);
}