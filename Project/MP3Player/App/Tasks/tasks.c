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
#include "PlayerCommand.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>

/************************************************************************************

Allocate the stacks for each task.
The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h

************************************************************************************/

//! Stack for the TouchPollingTask.
static OS_STK TouchPollingTaskStk[APP_CFG_TASK_START_STK_SIZE];
//! Stack for the StreamingTask.
static OS_STK StreamingTaskStk[APP_CFG_TASK_STK_SIZE];
//! Stack for the LcdHandlerTask.
static OS_STK LcdHandlerTaskStk[APP_CFG_TASK_STK_SIZE];
//! Stack for the CommandHandlerTask.
static OS_STK CommandHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE];
//! Stack for InitializationTask.
static OS_STK InitializationTaskStk[APP_CFG_TASK_START_STK_SIZE];

OS_FLAG_GRP * initFlags;            //!< Event flags used by the tasks to synchronize initialization.

/** @defgroup init_bits Initialization Event Bits
 * Event bits used by individual tasks to synchronize initialization across tasks.
 *
 * @{
 */
const uint32_t allEventBits             = 0xF;  //!< Event bit mask for all tasks together.
/** @} */

OS_EVENT * touch2CmdHandler;        //!< Message mailbox connecting TouchPollingTask to CommandHandlerTask.
OS_EVENT * touch2LcdHandler;        //!< Message mailbox connecting TouchPollingTask to LcdHandlerTask.
OS_EVENT * cmdHandler2Stream;       //!< Message mailbox connecting CommandHandlerTask to StreamingTask.
OS_EVENT * cmdHandler2LcdHandler;   //!< Message mailbox connecting CommandHandlerTask to LcdHandlerTask.
OS_EVENT * stream2LcdHandler;       //!< Message mailbox connecting StreamingTask to LcdHandlerTask.
OS_EVENT * progressMessage;         //!< Message mailbox used for sending progress information from StreamingTask to LcdHandlerTask.
OS_EVENT * semPrint;                //!< Binary semaphore allowing only a single task to print to the terminal at a time.

INPUT_COMMAND commandPressed;   //!< Used to store the state and control bit-masked values for the touch2CmdHandler message mailbox.
uint16_t touch2LcdMessage;      //!< Used to store the stop progress value and the max value for the touch2LcdHandler message mailbox.
COMMAND stateAndControl;        //!< Used to store the state and control bit-masked values for the cmdHandler2Stream and cmdHandler2LcdHandler message mailboxes.
char songTitle[SONGLEN];        //!< Used to store the song title string for the stream2LcdHandler message mailbox.
float progressValue = 0;        //!< Used to store the song progress value for the progressMessage message mailbox.

void StartupTask(void* pdata)
{
    uint8_t err;

    // Initialize all event pointers.
    touch2CmdHandler = OSMboxCreate(NULL);
    if(NULL == touch2CmdHandler) {
        PrintFormattedString("StartupTask: failed to create touch2CmdHandler\n");
    }

    touch2LcdHandler = OSMboxCreate(NULL);
    if(NULL == touch2LcdHandler) {
        PrintFormattedString("StartupTask: failed to create touch2LcdHandler\n");
    }

    cmdHandler2Stream = OSMboxCreate(NULL);
    if(NULL == cmdHandler2Stream) {
        PrintFormattedString("StartupTask: failed to create cmdHandler2Stream\n");
    }

    cmdHandler2LcdHandler = OSMboxCreate(NULL);
    if(NULL == cmdHandler2LcdHandler) {
        PrintFormattedString("StartupTask: failed to create cmdHandler2LcdHandler\n");
    }

    stream2LcdHandler = OSMboxCreate(NULL);
    if(NULL == stream2LcdHandler) {
        PrintFormattedString("StartupTask: failed to create stream2LcdHandler\n");
    }

    progressMessage = OSMboxCreate(NULL);
    if(NULL == stream2LcdHandler) {
        PrintFormattedString("StartupTask: failed to create progressMessage\n");
    }

    initFlags = OSFlagCreate((OS_FLAGS)0, &err);
    if(err != OS_ERR_NONE) {
        PrintFormattedString("StartupTask: failed to create initFlags with error %d\n", (uint32_t)err);
    }

    // Initialize the binary semaphore as full.
    semPrint = OSSemCreate(1);
    if(NULL == semPrint) {
        PrintFormattedString("StartupTask: failed to create semPrint\n");
    }

    // Start the system tick
    OS_CPU_SysTickInit(OS_TICKS_PER_SEC);

    // Create the tasks
    // The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h
    OSTaskCreate(StreamingTask, (void*)0, &StreamingTaskStk[APP_CFG_TASK_STK_SIZE-1], APP_TASK_STREAM_PRIO);
    OSTaskCreate(TouchPollingTask, (void*)0, &TouchPollingTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_TOUCH_PRIO);
    OSTaskCreate(CommandHandlerTask, (void*)0, &CommandHandlerTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_COMMAND_PRIO);
    OSTaskCreate(LcdHandlerTask, (void*)0, &LcdHandlerTaskStk[APP_CFG_TASK_STK_SIZE-1], APP_TASK_LCD_PRIO);
    OSTaskCreate(InitializationTask, (void*)0, &InitializationTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_INIT_PRIO);

    // Delete ourselves, letting the work be done in the new tasks.
	OSTaskDel(OS_PRIO_SELF);
}

void InitializationTask(void* pData)
{
	uint8_t err;

    // Wait for all tasks to be initialized.
    OSFlagPend(initFlags, allEventBits, OS_FLAG_WAIT_SET_ALL, 0, &err);
    if(OS_ERR_NONE != err) {
        PrintFormattedString("InitializationTask: pending on 0xF for flag group initFlags, err %d\n", (uint32_t)err);
    }

    OSFlagPost(initFlags, allEventBits, OS_FLAG_CLR, &err);
    if(OS_ERR_NONE != err) {
        PrintFormattedString("InitializationTask: posting 0x8 to flag group initFlags, err %d\n", (uint32_t)err);
    }

    // Delete ourselves, no further work to be done.
	OSTaskDel(OS_PRIO_SELF);
}