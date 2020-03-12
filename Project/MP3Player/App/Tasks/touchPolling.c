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
    const uint32_t delayTicks = OS_TICKS_PER_SEC / 12;
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

        if(OS_TRUE == touched && OS_TRUE == output) {
            output = OS_FALSE;

            BOOLEAN hit = OS_FALSE;
            uint8_t index;
            for(index = 0; index < btn_array_sz; ++index) {
                hit = checkForPress(p, index);
                if(OS_TRUE == hit) break;
            }

            if(OS_TRUE == hit) {
                BOOLEAN oneSecondElapsed = OS_FALSE;
                uint32_t startTime = OSTimeGet();
                while(touchCtrl.touched()) {
                    switch(commandPressed) {
                    case IC_PLAY:
                        //TODO: abstract this into function call
                        if(OS_FALSE == oneSecondElapsed) {
                            if(OSTimeGet() - startTime < OS_TICKS_PER_SEC) {
                                OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                            }
                            else {
                                // send some special command
                                touch2LcdMessage += 1;
                                if(OS_ERR_NONE != OSMboxPostOpt(touch2LcdHandler, &touch2LcdMessage, OS_POST_OPT_NONE)) {
                                    PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler (P1), aborting hold.\n");
                                    break;
                                }
                                oneSecondElapsed = OS_TRUE;
                                startTime = OSTimeGet();
                                OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                            }
                        }
                        else {
                            if(OSTimeGet() - startTime < PLAY_HOLD_TICKS) {
                                // send some special command
                                touch2LcdMessage += 1;
                                if(OS_ERR_NONE != OSMboxPostOpt(touch2LcdHandler, &touch2LcdMessage, OS_POST_OPT_NONE)) {
                                    PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler, aborting hold.\n");
                                    break;
                                }
                                OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                            }
                            else {
                                // set STOP command
                                commandPressed = IC_STOP;
                            }
                        }
                        break;
                    case IC_RESTART:
                        if(OSTimeGet() - startTime < OS_TICKS_PER_SEC) {
                            OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                        }
                        else {
                            // send the alternate command
                            commandPressed = IC_RWD;
                            oneSecondElapsed = OS_TRUE;
                        }
                        break;
                    case IC_RWD:
                        if(OS_ERR_NONE != OSMboxPostOpt(touch2CmdHandler, &commandPressed, OS_POST_OPT_NONE)) {
                            PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler, aborting hold.\n");
                            break;
                        }
                        OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                        break;
                    case IC_SKIP:
                        if(OSTimeGet() - startTime < OS_TICKS_PER_SEC) {
                            OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                        }
                        else {
                            // send the alternate command
                            commandPressed = IC_FF;
                            oneSecondElapsed = OS_TRUE;
                        }
                        break;
                    case IC_FF:
                        if(OS_ERR_NONE != OSMboxPostOpt(touch2CmdHandler, &commandPressed, OS_POST_OPT_NONE)) {
                            PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler, aborting hold.\n");
                            break;
                        }
                        OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                        break;
                    default:
                        OSTimeDly(delayTicks);
                        break;
                    }
                }

                // clear message count
                touch2LcdMessage = (uint16_t)(PLAY_HOLD_MAX_MSG << 8);
                if(OS_ERR_NONE != OSMboxPostOpt(touch2LcdHandler, &touch2LcdMessage, OS_POST_OPT_NONE)) {
                    PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler.\n");
                }

                if(OS_TRUE == oneSecondElapsed && (IC_STOP != commandPressed && IC_FF != commandPressed && IC_RWD != commandPressed)) {
                    continue;
                }

                // complete by sending a play command
                if(IC_FF == commandPressed || IC_RWD == commandPressed) {
                    commandPressed = IC_PLAY;
                }

                uint8_t err = OSMboxPostOpt(touch2CmdHandler, &commandPressed, OS_POST_OPT_NONE);
                if(OS_ERR_NONE != err) {
                    PrintFormattedString("TouchPollingTask: failed to post touch2CmdHandler with error %d\n", (INT32U)err);
                }
            }
        }
    }
}