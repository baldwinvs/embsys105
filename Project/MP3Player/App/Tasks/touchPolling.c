#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "Buttons.h"
#include "InputCommands.h"

#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>

#include <math.h>

//Globals
extern OS_FLAG_GRP *rxFlags;       // Event flags for synchronizing mailbox messages
extern OS_EVENT * touch2CmdHandler;
extern OS_EVENT * touch2LcdHandler;
extern INPUT_COMMAND commandPressed[1];
extern uint16_t touch2LcdMessage[1];

static const uint32_t PLAY_HOLD_TICKS = 2 * OS_TICKS_PER_SEC;
static const uint32_t PLAY_HOLD_DIVISOR = 20;
static const uint32_t PLAY_HOLD_MAX_MSG = (PLAY_HOLD_TICKS / OS_TICKS_PER_SEC) * PLAY_HOLD_DIVISOR;

Adafruit_FT6206 touchCtrl = Adafruit_FT6206(); // The touch controller

static long MapTouchToScreen(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
uint8_t checkForPress(const TS_Point p, const INT8U index)
{
    const BTN btn = btn_array[index];
    switch(btn.shape) {
    case S_CIRCLE:
        {
            const int32_t xDiff = p.x - btn.p0.p;
            const int32_t yDiff = p.y - btn.p1.p;
            const uint32_t dist = (uint32_t)sqrt((xDiff * xDiff) + (yDiff * yDiff));
            if(dist <= btn.p2.p) {
                commandPressed[0] = (INPUT_COMMAND)index;
                return 1;
            }
        }
        return 0;
    case S_SQUARE:
        {
            if(p.x >= btn.p0.p && p.x <= (btn.p0.p + btn.p2.p)) {
                if(p.y >= btn.p1.p && p.y <= (btn.p1.p + btn.h)) {
                    commandPressed[0] = (INPUT_COMMAND)index;
                    return 1;
                }
            }
        }
        return 0;
    case S_TRIANGLE:
    default:
        return 0;
    }
}

/************************************************************************************

Runs LCD/Touch demo code

************************************************************************************/
void TouchPollingTask(void* pData)
{
	PrintFormattedString("TouchPollingTask: starting\n");

    PrintFormattedString("Initializing FT6206 touchscreen controller\n");
    HANDLE hI2C1 = Open(PJDF_DEVICE_ID_I2C1, 0);
    PrintFormattedString("I2C1 handle opened\n");
    if(!PJDF_IS_VALID_HANDLE(hI2C1)) {
        PrintFormattedString("##! I2C1 PJDF HANDLE IS INVALID!!\nABORTING!\n\n");
        while(1);
    }

    touchCtrl.setPjdfHandle(hI2C1);
    PrintFormattedString("I2C handle set in touchCtrl\n");
    if (! touchCtrl.begin(40)) {  // pass in 'sensitivity' coefficient
        PrintFormattedString("Couldn't start FT6206 touchscreen controller\n");
        while (1);
    }
    else {
        INT8U err;
        //initialized
        OSFlagPost(rxFlags, 2, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("TouchPollingTask: posting to flag group with error code %d\n", (INT32U)err);
        }

        //wait for other tasks for finish initialization
        OSFlagPend(rxFlags, 2, OS_FLAG_WAIT_CLR_ALL, 0, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("TouchPollingTask: pending on 0x2 value for flag event with error code %d\n", (INT32U)err);
        }
    }

    uint32_t output = 1;
    const uint32_t delayTicks = OS_TICKS_PER_SEC / 12;
    TS_Point rawPoint;
    TS_Point p;
    touch2LcdMessage[0] = {(uint16_t)(PLAY_HOLD_MAX_MSG << 8)};

    while (1) {
        boolean touched = false;

        if(touchCtrl.touched()) {
            touched = true;
        }
        else {
            output = 1;
        }

        if(!touched) {
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

        if(1 == touched && 1 == output) {
            output = 0;

            uint8_t hit = 0;
            uint8_t index;
            for(index = 0; index < btn_array_sz; ++index) {
                hit = checkForPress(p, index);
                if(1 == hit) break;
            }

            if(1 == hit) {
                uint8_t oneSecondElapsed = 0;
                uint32_t startTime = OSTimeGet();
                while(touchCtrl.touched()) {
                    switch(commandPressed[0]) {
                    case IC_PLAY:
                        //TODO: abstract this into function call
                        if(0 == oneSecondElapsed) {
                            if(OSTimeGet() - startTime < OS_TICKS_PER_SEC) {
                                OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                            }
                            else {
                                // send some special command
                                touch2LcdMessage[0] += 1;
                                if(OS_ERR_NONE != OSMboxPostOpt(touch2LcdHandler, touch2LcdMessage, OS_POST_OPT_NONE)) {
                                    PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler (P1), aborting hold.\n");
                                    break;
                                }
                                oneSecondElapsed = 1;
                                startTime = OSTimeGet();
                                OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                            }
                        }
                        else {
                            if(OSTimeGet() - startTime < PLAY_HOLD_TICKS) {
                                // send some special command
                                touch2LcdMessage[0] += 1;
                                if(OS_ERR_NONE != OSMboxPostOpt(touch2LcdHandler, touch2LcdMessage, OS_POST_OPT_NONE)) {
                                    PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler, aborting hold.\n");
                                    break;
                                }
                                OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                            }
                            else {
                                // set STOP command
                                commandPressed[0] = IC_STOP;
                            }
                        }
                        break;
                    case IC_RESTART:
                        if(OSTimeGet() - startTime < OS_TICKS_PER_SEC) {
                            OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                        }
                        else {
                            // send the alternate command
                            commandPressed[0] = IC_RWD;
                            oneSecondElapsed = 1;
                        }
                        break;
                    case IC_RWD:
                        if(OS_ERR_NONE != OSMboxPostOpt(touch2CmdHandler, commandPressed, OS_POST_OPT_NONE)) {
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
                            commandPressed[0] = IC_FF;
                            oneSecondElapsed = 1;
                        }
                        break;
                    case IC_FF:
                        if(OS_ERR_NONE != OSMboxPostOpt(touch2CmdHandler, commandPressed, OS_POST_OPT_NONE)) {
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
                touch2LcdMessage[0] = (uint16_t)(PLAY_HOLD_MAX_MSG << 8);
                if(OS_ERR_NONE != OSMboxPostOpt(touch2LcdHandler, touch2LcdMessage, OS_POST_OPT_NONE)) {
                    PrintFormattedString("TouchPollingTask: failed to post touch2LcdHandler.\n");
                }

                if(oneSecondElapsed && (IC_STOP != commandPressed[0] && IC_FF != commandPressed[0] && IC_RWD != commandPressed[0])) {
                    continue;
                }

                // complete by sending a play command
                if(IC_FF == commandPressed[0] || IC_RWD == commandPressed[0]) {
                    commandPressed[0] = IC_PLAY;
                }

                uint8_t err = OSMboxPostOpt(touch2CmdHandler, commandPressed, OS_POST_OPT_NONE);
                if(OS_ERR_NONE != err) {
                    PrintFormattedString("TouchPollingTask: failed to post touch2CmdHandler with error %d\n", (INT32U)err);
                }
            }
        }
    }
}