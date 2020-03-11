/************************************************************************************

Copyright (c) 2001-2016  University of Washington Extension.  All rights reserved.

Module Name:

    main.c

Module Description:

    Entry point for MP3 Player Demo application

Revision History:
2016/2 Nick Strathy adapted it for NUCLEO-F401RE

************************************************************************************/

#include "bsp.h"
#include "print.h"

/** @brief The initial task running, started by main(); it starts the system tick timer
 * and creates all the other tasks and then it deletes itself.
 * 
 * @param pData Pointer to the input data for the task.
 */
void StartupTask(void* pdata);

//! Stack for StartupTask.
static OS_STK StartupStk[APP_CFG_TASK_START_STK_SIZE];

// Allocate the print buffer
PRINT_DEFINEBUFFER();

// General purpose delay
void delay(uint32_t count)
{
    while(count--);
}

/*###################################################################################

Routine Description:

    Application entry point.

Arguments:

    None.

Return Value:

    none.

###################################################################################*/
void main() {
    uint8_t err;
    Hw_init();

    delay(1000000);

    RETAILMSG(1, ("MP3 Player Demo: Built %s %s.\r\n\r\n",
        __DATE__,
        __TIME__));


    // Initialize the OS
    DEBUGMSG(1, ("main: Running OSInit()...\n"));
    OSInit();

    // Initialize driver framework after initializing uCOS since the framework uses uCOS services
    DEBUGMSG(1, ("Initializing PJDF driver framework...\n"));
    InitPjdf();

    // Create the startup task
    DEBUGMSG(1, ("main: Creating start up task.\n"));

    err = OSTaskCreate(
        StartupTask,
        (void*)0,
        &StartupStk[APP_CFG_TASK_START_STK_SIZE-1],
        APP_TASK_START_PRIO);

    if (err != OS_ERR_NONE) {
        DEBUGMSG(1, ("main: failed creating start up task: %d\n", (uint32_t)err));
        while(OS_TRUE);  //park on error
    }

    DEBUGMSG(1, ("Starting multi-tasking.\n"));

    // start the OS
    OSStart();

    // should never reach here
    while (1);
}
