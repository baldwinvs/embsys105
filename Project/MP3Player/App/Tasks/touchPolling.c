#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "Buttons.h"
#include "InputCommands.h"

#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>

#define BUFSIZE 256
#define PENRADIUS 3

//GLOBALS
extern OS_EVENT * touch2CmdHandler;
extern INPUT_COMMAND commandPressed[1];

static char buf[BUFSIZE];

Adafruit_FT6206 touchCtrl = Adafruit_FT6206(); // The touch controller

static long MapTouchToScreen(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static uint8_t checkForButtonPress(const TS_Point p, const size_t index)
{
    if(p.x >= btn_array[index].x && p.x <= (btn_array[index].x + btn_array[index].w)) {
        if(p.y >= btn_array[index].y && p.y <= (btn_array[index].y + btn_array[index].h)) {
            commandPressed[0] = (INPUT_COMMAND)index;
            uint8_t err = OSMboxPostOpt(touch2CmdHandler, commandPressed, OS_POST_OPT_NONE);
            if(OS_ERR_NONE != err) {
                PrintWithBuf(buf, BUFSIZE, "TouchPollingTask: failed to post touch2CmdHandler with error %d\n", (INT32U)err);
            }
            return 1;
        }
    }
    return 0;
}

/************************************************************************************

Runs LCD/Touch demo code

************************************************************************************/
void TouchPollingTask(void* pData)
{
	PrintWithBuf(buf, BUFSIZE, "TouchPollingTask: starting\n");

    PrintWithBuf(buf, BUFSIZE, "Initializing FT6206 touchscreen controller\n");
    HANDLE hI2C1 = Open(PJDF_DEVICE_ID_I2C1, 0);
    PrintWithBuf(buf, BUFSIZE, "I2C1 handle opened\n");
    if(!PJDF_IS_VALID_HANDLE(hI2C1)) {
        PrintWithBuf(buf, BUFSIZE, "##! I2C1 PJDF HANDLE IS INVALID!!\nABORTING!\n\n");
        while(1);
    }

    touchCtrl.setPjdfHandle(hI2C1);
    PrintWithBuf(buf, BUFSIZE, "I2C handle set in touchCtrl\n");
    if (! touchCtrl.begin(40)) {  // pass in 'sensitivity' coefficient
        PrintWithBuf(buf, BUFSIZE, "Couldn't start FT6206 touchscreen controller\n");
        while (1);
    }
    else {
        PrintWithBuf(buf, BUFSIZE, "## FT6206 touchscreen controller started successfully!\n");
    }

    uint32_t output = 1;
    const uint32_t delayTicks = OS_TICKS_PER_SEC / 12;

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

        TS_Point rawPoint;

        rawPoint = touchCtrl.getPoint();

        if (rawPoint.x == 0 && rawPoint.y == 0)
        {
            continue; // usually spurious, so ignore
        }

        // transform touch orientation to screen orientation.
        TS_Point p = TS_Point();
        p.x = MapTouchToScreen(rawPoint.x, 0, ILI9341_TFTWIDTH, ILI9341_TFTWIDTH, 0);
        p.y = MapTouchToScreen(rawPoint.y, 0, ILI9341_TFTHEIGHT, ILI9341_TFTHEIGHT, 0);

        while(touchCtrl.touched()) {
            OSTimeDly(delayTicks);
        }

        if(1 == touched && 1 == output) {
            output = 0;

            uint8_t hit = 0;

            size_t i = 0;
            while(0 == hit && i < btn_array_sz) {
                hit = checkForButtonPress(p, i++);
            }
        }
    }
}