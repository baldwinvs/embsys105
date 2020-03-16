#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "Buttons.h"
#include "InputCommands.h"

#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>

#include <math.h>

//Globals
extern OS_FLAG_GRP *initFlags;
extern OS_EVENT * touch2CmdHandler;
extern OS_EVENT * touch2LcdHandler;
extern INPUT_COMMAND commandPressed;
extern uint16_t touch2LcdMessage;

/** @addtogroup init_bits
 * @{
 */
const uint32_t touchPollingEventBit     = 0x2;  //!< Event bit for the TouchPollingTask.
/** @} */

//! The number of ticks to activate the STOP command.
static const uint32_t PLAY_HOLD_TICKS = 2 * OS_TICKS_PER_SEC;
//! Divisor of OS_TICKS_PER_SEC to get a .05 second delay.
static const uint32_t PLAY_HOLD_DIVISOR = 20;
//! The maximum number of messages to send to LcdHandlerTask.
static const uint32_t PLAY_HOLD_MAX_MSG = (PLAY_HOLD_TICKS / OS_TICKS_PER_SEC) * PLAY_HOLD_DIVISOR;

static BOOLEAN sendStop = OS_FALSE;
static BOOLEAN sendFastForward = OS_FALSE;
static BOOLEAN sendRewind = OS_FALSE;
static const uint32_t delayTicks = OS_TICKS_PER_SEC / 12;

Adafruit_FT6206 touchCtrl = Adafruit_FT6206(); // The touch controller

static long MapTouchToScreen(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/** Cycle through the buttons defined in btn_array.
 *
 * @note Does not have an implementation for triangle buttons currently.
 *
 * @param p The input touch screen point.
 * @param index The btn_array index to check for a press.
 *
 * @returns OS_TRUE if a button was pressed, OS_FALSE otherwise.
 */
static BOOLEAN checkForPress(const TS_Point p, const uint8_t index);

/** @brief Handles logic when holding down the PLAY/SKIP/RESTART buttons.
 *
 * @param oneSecElapsed Pointer to the flag indicating that the button has been pressed for 1 second.
 * @param start Pointer to the start time; used for determining if a button has been pressed long enough.
 */
static void handlePressedButton(BOOLEAN * oneSecElapsed, uint32_t * start);

void TouchPollingTask(void* pData)
{
    HANDLE hI2C1 = Open(PJDF_DEVICE_ID_I2C1, 0);
    if(!PJDF_IS_VALID_HANDLE(hI2C1)) {
        PrintFormattedString("Failure opening I2C driver: %s\n", PJDF_DEVICE_ID_I2C1);
        while(1);
    }

    touchCtrl.setPjdfHandle(hI2C1);
    if (! touchCtrl.begin(40)) {  // pass in 'sensitivity' coefficient
        PrintFormattedString("Couldn't start FT6206 touchscreen controller\n");
        while (1);
    }
    else {
        uint8_t err;
        //initialized
        OSFlagPost(initFlags, touchPollingEventBit, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("TouchPollingTask: posting to flag group with error code %d\n", (uint32_t)err);
        }

        //wait for other tasks for finish initialization
        OSFlagPend(initFlags, touchPollingEventBit, OS_FLAG_WAIT_CLR_ALL, 0, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("TouchPollingTask: pending on 0x2 value for flag event with error code %d\n", (uint32_t)err);
        }
    }

    BOOLEAN output = OS_TRUE;
    TS_Point rawPoint;
    TS_Point p;
    touch2LcdMessage = (uint16_t)(PLAY_HOLD_MAX_MSG << 8);

    while (OS_TRUE) {
        BOOLEAN touched = OS_FALSE;

        if(touchCtrl.touched()) {
            touched = OS_TRUE;
        }
        else {
            output = OS_TRUE;
        }

        if(OS_FALSE == touched) {
            OSTimeDly(delayTicks);
            continue;
        }

        rawPoint = touchCtrl.getPoint();

        if (rawPoint.x == 0 && rawPoint.y == 0)
        {
            continue; // usually spurious, so ignore
        }

        // transform touch orientation to screen orientation.
        p.x = MapTouchToScreen(rawPoint.x, 0, ILI9341_TFTWIDTH, ILI9341_TFTWIDTH, 0);
        p.y = MapTouchToScreen(rawPoint.y, 0, ILI9341_TFTHEIGHT, ILI9341_TFTHEIGHT, 0);

        // Only handle once per touch.
        if(OS_TRUE == touched && OS_TRUE == output) {
            output = OS_FALSE;

            BOOLEAN hit = OS_FALSE;
            uint8_t index;

            // Check all buttons for press; exit loop if found.
            for(index = 0; index < btn_array_sz; ++index) {
                hit = checkForPress(p, index);
                if(OS_TRUE == hit) break;
            }

            if(OS_TRUE == hit) {
                BOOLEAN oneSecondElapsed = OS_FALSE;
                uint32_t startTime = OSTimeGet();

                // While continuing to hold the press, handle special behavior if necessary.
                while(touchCtrl.touched()) {
                    handlePressedButton(&oneSecondElapsed, &startTime);
                }

                // Send the stop progress with a current value of 0 to begin clearing the progress bars.
                touch2LcdMessage = (uint16_t)(PLAY_HOLD_MAX_MSG << 8);
                if(OS_ERR_NONE != OSMboxPostOpt(touch2LcdHandler, &touch2LcdMessage, OS_POST_OPT_NONE)) {
                    PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler.\n");
                }

                // Play button was held for less than 3 seconds, don't send a command.
                if(OS_TRUE == oneSecondElapsed && OS_FALSE == sendStop) {
                    continue;
                }

                // Complete FF/RWD sequence by sending a play command.
                if(OS_TRUE == sendFastForward || OS_TRUE == sendRewind) {
                    commandPressed = IC_PLAY;
                }

                uint8_t err = OSMboxPostOpt(touch2CmdHandler, &commandPressed, OS_POST_OPT_NONE);
                if(OS_ERR_NONE != err) {
                    PrintFormattedString("TouchPollingTask: failed to post touch2CmdHandler with error %d\n", (INT32U)err);
                }

                // Reset the flags.
                sendStop = OS_FALSE;
                sendFastForward = OS_FALSE;
                sendRewind = OS_FALSE;
            }
        }
    }
}

static BOOLEAN checkForPress(const TS_Point p, const uint8_t index)
{
    const BTN btn = btn_array[index];
    switch(btn.shape) {
    case S_CIRCLE:
        {
            const int32_t xDiff = p.x - btn.p0.p;
            const int32_t yDiff = p.y - btn.p1.p;
            const uint32_t dist = (uint32_t)sqrt((xDiff * xDiff) + (yDiff * yDiff));
            if(dist <= btn.p2.p) {
                commandPressed = (INPUT_COMMAND)index;
                return OS_TRUE;
            }
        }
        return OS_FALSE;
    case S_SQUARE:
        {
            if(p.x >= btn.p0.p && p.x <= (btn.p0.p + btn.p2.p)) {
                if(p.y >= btn.p1.p && p.y <= (btn.p1.p + btn.h)) {
                    commandPressed = (INPUT_COMMAND)index;
                    return OS_TRUE;
                }
            }
        }
        return OS_FALSE;
    case S_TRIANGLE:
    default:
        return OS_FALSE;
    }
}

static void handlePressedButton(BOOLEAN * oneSecElapsed, uint32_t * start)
{
    switch(commandPressed) {
    case IC_PLAY:
        if(OS_FALSE == *oneSecElapsed) {
            if(OSTimeGet() - *start < OS_TICKS_PER_SEC) {
                OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
            }
            else {
                // Send the first stop progress value.
                touch2LcdMessage += 1;
                if(OS_ERR_NONE != OSMboxPostOpt(touch2LcdHandler, &touch2LcdMessage, OS_POST_OPT_NONE)) {
                    PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler (P1), aborting hold.\n");
                    break;
                }
                *oneSecElapsed = OS_TRUE;
                *start = OSTimeGet();
                OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
            }
        }
        else {
            if(OSTimeGet() - *start < PLAY_HOLD_TICKS) {
                // Send additional stop progress values until the max time is reached.
                touch2LcdMessage += 1;
                if(OS_ERR_NONE != OSMboxPostOpt(touch2LcdHandler, &touch2LcdMessage, OS_POST_OPT_NONE)) {
                    PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler, aborting hold.\n");
                    break;
                }
                OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
            }
            else {
                // Finally set the command to stop; will be sent when not touching the screen.
                commandPressed = IC_STOP;
                sendStop = OS_TRUE;
            }
        }
        break;
    case IC_RESTART:
        if(OSTimeGet() - *start < OS_TICKS_PER_SEC) {
            OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
        }
        else {
            // Change command to rewind state.
            commandPressed = IC_RWD;
            sendRewind = OS_TRUE;
        }
        break;
    case IC_RWD:
        // Send a rewind state at every iteration.
        if(OS_ERR_NONE != OSMboxPostOpt(touch2CmdHandler, &commandPressed, OS_POST_OPT_NONE)) {
            PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler, aborting hold.\n");
            break;
        }
        OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
        break;
    case IC_SKIP:
        if(OSTimeGet() - *start < OS_TICKS_PER_SEC) {
            OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
        }
        else {
            // Change command to fast forward state.
            commandPressed = IC_FF;
            sendFastForward = OS_TRUE;
        }
        break;
    case IC_FF:
        // Send a fast forward state at every iteration.
        if(OS_ERR_NONE != OSMboxPostOpt(touch2CmdHandler, &commandPressed, OS_POST_OPT_NONE)) {
            PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler, aborting hold.\n");
            break;
        }
        OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
        break;
    default:
        // For any other button, delay until released.
        OSTimeDly(delayTicks);
        break;
    }
}