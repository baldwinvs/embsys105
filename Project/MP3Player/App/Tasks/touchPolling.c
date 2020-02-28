#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "Buttons.h"
#include "InputCommands.h"

#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>

#include <math.h>

#define PENRADIUS 3

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
        OSFlagPost(rxFlags, 2, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("TouchPollingTask: posting to flag group with error code %d\n", (INT32U)err);
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
        p = TS_Point();
        p.x = MapTouchToScreen(rawPoint.x, 0, ILI9341_TFTWIDTH, ILI9341_TFTWIDTH, 0);
        p.y = MapTouchToScreen(rawPoint.y, 0, ILI9341_TFTHEIGHT, ILI9341_TFTHEIGHT, 0);

        if(1 == touched && 1 == output) {
            output = 0;

            uint8_t hit = 0;

            //PLAY CIRCLE
            {
                int32_t xDiff = p.x - play_circle.x;
                int32_t yDiff = p.y - play_circle.y;
                uint32_t distance = sqrt((xDiff * xDiff) + (yDiff * yDiff));
                if(distance <= play_circle.r) {
                    commandPressed[0] = IC_PLAY;
                    hit = 1;
                    goto check_hold;
                }
            }

            if(p.x >= restart_square.x && p.x <= (restart_square.x + restart_square.w)) {
                if(p.y >= restart_square.y && p.y <= (restart_square.y + restart_square.h)) {
                    commandPressed[0] = IC_RESTART;
                    hit = 1;
                    goto check_hold;
                }
            }

            if(p.x >= skip_square.x && p.x <= (skip_square.x + skip_square.w)) {
                if(p.y >= skip_square.y && p.y <= (skip_square.y + skip_square.h)) {
                    commandPressed[0] = IC_SKIP;
                    hit = 1;
                    goto check_hold;
                }
            }

            if(p.x >= vol_dwn.x && p.x <= (vol_dwn.x + vol_dwn.w)) {
                if(p.y >= vol_dwn.y && p.y <= (vol_dwn.y + vol_dwn.h)) {
                    commandPressed[0] = IC_VOLDOWN;
                    hit = 1;
                    goto check_hold;
                }
            }

            if(p.x >= vol_up.x && p.x <= (vol_up.x + vol_up.w)) {
                if(p.y >= vol_up.y && p.y <= (vol_up.y + vol_up.h)) {
                    commandPressed[0] = IC_VOLUP;
                    hit = 1;
                    goto check_hold;
                }
            }
        check_hold:
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
                                    PrintFormattedString("TouchPollinTask: failed to post touch2LcdHandler (P1), aborting hold.\n");
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
                                    PrintFormattedString("TouchPollinTask: failed to post touch2LcdHandler, aborting hold.\n");
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
                        if(0 == oneSecondElapsed) {
                            if(OSTimeGet() - startTime < OS_TICKS_PER_SEC) {
                                OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                            }
                            else {
                                // send the alternate command
                                commandPressed[0] = IC_RWD;
                                oneSecondElapsed = 1;
                            }
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
                        if(0 == oneSecondElapsed) {
                            if(OSTimeGet() - startTime < OS_TICKS_PER_SEC) {
                                OSTimeDly(OS_TICKS_PER_SEC / PLAY_HOLD_DIVISOR);
                            }
                            else {
                                // send the alternate command
                                commandPressed[0] = IC_FF;
                                oneSecondElapsed = 1;
                            }
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
                    PrintFormattedString("TouchPollinTask: failed to post touch2LcdHandler.\n");
                }

                if(oneSecondElapsed && (IC_STOP != commandPressed[0])) {
                    continue;
                }

                uint8_t err = OSMboxPostOpt(touch2CmdHandler, commandPressed, OS_POST_OPT_NONE);
                if(OS_ERR_NONE != err) {
                    PrintFormattedString("TouchPollingTask: failed to post touch2CmdHandler with error %d\n", (INT32U)err);
                }
            }
        }
    }
}