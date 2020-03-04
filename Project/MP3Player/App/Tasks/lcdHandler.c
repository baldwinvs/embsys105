#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "Buttons.h"
#include "InputCommands.h"
#include "PlayerControl.h"

//Globals
extern OS_FLAG_GRP *rxFlags;       // Event flags for synchronizing mailbox messages
extern OS_EVENT * touch2LcdHandler;
extern OS_EVENT * cmdHandler2LcdHandler;
extern OS_EVENT * stream2LcdHandler;
extern OS_EVENT * progressMessage;

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>

Adafruit_ILI9341 lcdCtrl = Adafruit_ILI9341(); // The LCD controller

#define BURNT_ORANGE        (0xCAA0)    /* 255, 165,   0 */
#define DARK_BURNT_ORANGE   (0x7980)    /* 122,  85,   0 */

static const size_t maxTrackNameSize = 64;

static const int16_t statePos_Y = 140;
static const int16_t initializingPos_X = (120 - 60 - 12);    // center x pos [120] - (5 * char count) - char count
static const int16_t initializedPos_X = (120 - 55 - 11);     // center x pos [120] - (5 * char count)
static const int16_t playingPos_X = (120 - 35 - 7);         // center x pos [120] - (5 * char count)
static const int16_t stoppedPos_X = (120 - 35 - 7);         // center x pos [120] - (5 * char count)
static const int16_t pausedPos_X  = (120 - 30 - 6);         // center x pos [120] - (5 * char count)
static const int16_t fastForwardingPos_X = (120 - 75 - 15);  // center x pos [120] - (5 * char count)
static const int16_t rewindingPos_X = (120 - 45 - 9);       // center x pos [120] - (5 * char count)

static char buf[maxTrackNameSize];
static uint8_t sliderSpacing = 16;
static const uint8_t maxSliderPosition = 196;
static uint8_t volume = 4;  // range: [0-10], each value represents 10%

static const float progressScaleFactor = innerProgress_square.p2.p / 100.f;
static uint8_t previousProgress = 0;

// Renders a character at the current cursor position on the LCD
static void PrintCharToLcd(char c)
{
    lcdCtrl.write(c);
}

/************************************************************************************

Print a formated string with the given buffer to LCD.
Each task should use its own buffer to prevent data corruption.

************************************************************************************/
void PrintToLcdWithBuf(char *buf, int size, char *format, ...)
{
    va_list args;
    va_start(args, format);
    PrintToDeviceWithBuf(PrintCharToLcd, buf, size, format, args);
    va_end(args);
}

static void drawButton(const BTN btn, const uint16_t color, const uint16_t radius)
{
    switch(btn.shape) {
    case S_CIRCLE:
        lcdCtrl.fillCircle(btn.p0.p, btn.p1.p, btn.p2.p, color);
        break;
    case S_SQUARE:
        lcdCtrl.fillRoundRect(btn.p0.p, btn.p1.p, btn.p2.p, btn.h, radius, color);
        break;
    case S_TRIANGLE:
        lcdCtrl.fillTriangle(btn.p0.tp.x, btn.p0.tp.y, btn.p1.tp.x, btn.p1.tp.y, btn.p2.tp.x, btn.p2.tp.y, color);
        break;
    default:
        break;
    }
}

static void DrawLcdContents()
{
    const char* volume_up = "+";
    const char* volume_down = "-";

    lcdCtrl.fillScreen(ILI9341_BLACK);
    lcdCtrl.setTextColor(BURNT_ORANGE);

    lcdCtrl.setTextSize(2);
    lcdCtrl.setCursor(initializingPos_X, statePos_Y);
    PrintToLcdWithBuf(buf, 16, "INITIALIZING");

    lcdCtrl.setTextSize(4);

    //don't draw volume down rectangle, do draw text
    lcdCtrl.setCursor(vol_dwn.p0.p + vol_dwn.p2.p/4, vol_dwn.p1.p + 6);
    PrintToLcdWithBuf(buf, 12, (char*)volume_down);

    drawButton(vol_bar, ILI9341_CYAN, 0);
    drawButton(vol_slider, BURNT_ORANGE, 2);

    //don't draw volume up rectangle, do draw text
    lcdCtrl.setCursor(vol_up.p0.p + vol_up.p2.p/4, vol_up.p1.p + 6);
    PrintToLcdWithBuf(buf, 12, (char*)volume_up);

    drawButton(progress_square, ILI9341_CYAN, 4);
    drawButton(innerProgress_square, ILI9341_BLACK, 0);

    drawButton(restart_square, BURNT_ORANGE, 5);
    const BTN restart_square_inner = {S_SQUARE, restart_square.p0.p+2, restart_square.p1.p+2, restart_square.p2.p-4, restart_square.h-4};
    drawButton(restart_square_inner, ILI9341_CYAN, 5);
    drawButton(skip_square, BURNT_ORANGE, 5);
    const BTN skip_square_inner = {S_SQUARE, skip_square.p0.p+2, skip_square.p1.p+2, skip_square.p2.p-4, skip_square.h-4};
    drawButton(skip_square_inner, ILI9341_CYAN, 5);

    drawButton(play_circle, BURNT_ORANGE, 0);
    const BTN play_circle_inner = {S_CIRCLE, play_circle.p0.p, play_circle.p1.p, play_circle.p2.p-3, 0};
    drawButton(play_circle_inner, ILI9341_CYAN, 0);

    drawButton(play_triangle, BURNT_ORANGE, 0);

    drawButton(restart_left_triangle, BURNT_ORANGE, 0);
    drawButton(restart_right_triangle, BURNT_ORANGE, 0);
    drawButton(restart_right_square, BURNT_ORANGE, 0);

    drawButton(skip_left_triangle, BURNT_ORANGE, 0);
    drawButton(skip_right_triangle, BURNT_ORANGE, 0);
    drawButton(skip_right_square, BURNT_ORANGE, 0);

    lcdCtrl.setTextSize(2);
}

static void drawPause(const uint16_t color)
{
    drawButton(pause_left_square, color, 1);
    drawButton(pause_right_square, color, 1);
}
void drawPlay(const uint16_t color)
{
    drawButton(play_triangle, color, 0);
}

static void updateTrackName(const char* prevTrack, const char* newTrack)
{
    lcdCtrl.setCursor(40, 105);
    lcdCtrl.setTextColor(ILI9341_BLACK);
    PrintToLcdWithBuf(buf, maxTrackNameSize, (char*)prevTrack);

    lcdCtrl.setCursor(40, 105);
    lcdCtrl.setTextColor(BURNT_ORANGE);
    PrintToLcdWithBuf(buf, maxTrackNameSize, (char*)newTrack);
}

static void incrementVolumeSlider(void)
{
    if(volume > 0) {
        //fill in the previous slider position
        drawButton(vol_slider, ILI9341_BLACK, 2);
        //redraw the volume bar that was drawn over
        size_t i;
        for(i = vol_bar.p1.p; i < vol_bar.p1.p + vol_bar.h; ++i) {
            lcdCtrl.drawFastHLine(vol_slider.p0.p, i, vol_slider.p2.p, ILI9341_CYAN);
        }

        --volume;
    }
}

static void decrementVolumeSlider(void)
{
    if(volume < 10) {
        //fill in the previous slider position
        drawButton(vol_slider, ILI9341_BLACK, 2);
        //redraw the volume bar that was drawn over
        size_t i;
        for(i = vol_bar.p1.p; i < vol_bar.p1.p + vol_bar.h; ++i) {
            lcdCtrl.drawFastHLine(vol_slider.p0.p, i, vol_slider.p2.p, ILI9341_CYAN);
        }

        ++volume;
    }
}

static void updateVolumeSlider(BOOLEAN volumeUp)
{
    if(OS_TRUE == volumeUp) {
        incrementVolumeSlider();
    }
    else {
        decrementVolumeSlider();
    }
    vol_slider.p0.p = maxSliderPosition - (sliderSpacing * volume);
    drawButton(vol_slider, BURNT_ORANGE, 2);
}

static void updateProgressBar(const float progress, const CONTROL state)
{
    const uint8_t scaledProgress = (uint8_t)(progress * progressScaleFactor);

    //play, ff
    if(scaledProgress > previousProgress) {
        do {
            lcdCtrl.drawFastVLine(innerProgress_square.p0.p + previousProgress, innerProgress_square.p1.p,
                                  innerProgress_square.h, BURNT_ORANGE);
        }
        while(++previousProgress < scaledProgress);
        lcdCtrl.drawFastVLine(innerProgress_square.p0.p + scaledProgress, innerProgress_square.p1.p,
                              innerProgress_square.h, BURNT_ORANGE);
    }
    //rwd
    else if (scaledProgress < previousProgress) {
        if(scaledProgress == 0) {
            drawButton(innerProgress_square, ILI9341_BLACK, 0);
            previousProgress = scaledProgress;
        }
        else {
            do {
                lcdCtrl.drawFastVLine(innerProgress_square.p0.p + previousProgress, innerProgress_square.p1.p,
                                      innerProgress_square.h, ILI9341_BLACK);
            }
            while(--previousProgress > scaledProgress);
        }
    }
}

void LcdHandlerTask(void* pData)
{
    PjdfErrCode pjdfErr;
    INT32U length;
#if DEBUG
    PrintFormattedString("LcdHandlerTask: starting\n");

	PrintFormattedString("Opening LCD driver: %s\n", PJDF_DEVICE_ID_LCD_ILI9341);
#endif
    // Open handle to the LCD driver
    HANDLE hLcd = Open(PJDF_DEVICE_ID_LCD_ILI9341, 0);
    if (!PJDF_IS_VALID_HANDLE(hLcd)) while(1);

#if DEBUG
	PrintFormattedString("Opening LCD SPI driver: %s\n", LCD_SPI_DEVICE_ID);
#endif
    // We talk to the LCD controller over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the LCD driver.
    HANDLE hSPI = Open(LCD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hLcd, PJDF_CTRL_LCD_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);

#if DEBUG
	PrintFormattedString("Initializing LCD controller\n");
#endif
    lcdCtrl.setPjdfHandle(hLcd);
    lcdCtrl.begin();

    lcdCtrl.setRotation(0);

    DrawLcdContents();

    {
        INT8U err;
        OSFlagPost(rxFlags, 8, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
#if DEBUG
            PrintFormattedString("LcdHandlerTask: posting to flag group with error code %d\n", (INT32U)err);
#endif
            while(1);
        }

        OSFlagPend(rxFlags, 8, OS_FLAG_WAIT_CLR_ALL, 0, &err);
        if(OS_ERR_NONE != err) {
#if DEBUG
            PrintFormattedString("LcdHandlerTask: pending on flag group (bit 0x8) with error code %d\n", (INT32U)err);
#endif
            while(1);
        }
    }


    lcdCtrl.setTextColor(ILI9341_BLACK);
    lcdCtrl.setCursor(initializingPos_X, statePos_Y);
    PrintToLcdWithBuf(buf, 16, "INITIALIZING");

    lcdCtrl.setTextColor(BURNT_ORANGE);
    lcdCtrl.setCursor(initializedPos_X, statePos_Y);
    PrintToLcdWithBuf(buf, 16, "INITIALIZED");

    OSTimeDly(OS_TICKS_PER_SEC);

    lcdCtrl.setTextColor(ILI9341_BLACK);
    lcdCtrl.setCursor(initializedPos_X, statePos_Y);
    PrintToLcdWithBuf(buf, 16, "INITIALIZED");

    lcdCtrl.setTextColor(BURNT_ORANGE);
    lcdCtrl.setCursor(stoppedPos_X, statePos_Y);
    PrintToLcdWithBuf(buf, 16, "STOPPED");

    uint32_t* msgReceived = NULL;
    uint8_t stopped = 0;
    uint8_t lastCount = 0;
    uint8_t paused = 0;
    uint8_t flipFlop = 0;
    CONTROL state = PC_STOP;
    CONTROL prevState = PC_STOP;
    CONTROL control = PC_NONE;
    char prevTrackName[maxTrackNameSize] = {0};

    while(1) {
        msgReceived = (uint32_t*)OSMboxAccept(touch2LcdHandler);
        if(NULL != msgReceived) {
            // 0xAABB, AA is max count, BB is current count
            uint8_t max = (*msgReceived >> 8) & 0x00FF;
            uint8_t count = *msgReceived & 0x00FF;

            if(count % (max + 1) != 0) {
                const uint8_t adjustedCount = 4 * count;
                //Draw the lower left progress bar
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount - 3), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount - 2), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount - 1), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount),     5, BURNT_ORANGE);
                //Draw the lower right progress bar
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount - 3), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount - 2), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount - 1), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount),     5, BURNT_ORANGE);
                //Draw the upper left progress bar
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount - 3), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount - 2), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount - 1), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount),     5, BURNT_ORANGE);
                //Draw the upper right progress bar
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount - 3), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount - 2), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount - 1), 5, BURNT_ORANGE);
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount),     5, BURNT_ORANGE);
                lastCount = count;
            }
            else {
                stopped = 1;
            }
        }

        msgReceived = (uint32_t*)OSMboxAccept(cmdHandler2LcdHandler);
        if(NULL != msgReceived) {
            state = (CONTROL)(*msgReceived & stateMask);
            control = (CONTROL)(*msgReceived & controlMask);
            switch(state) {
            case PC_STOP:
                if(prevState != state) {
                    lcdCtrl.setTextColor(ILI9341_BLACK);
                    switch((CONTROL)prevState) {
                    case PC_PLAY:
                        lcdCtrl.setCursor(playingPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "PLAYING");
                        break;
                    case PC_PAUSE:
                        lcdCtrl.setCursor(pausedPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "PAUSED");
                        break;
                    default:
                        break;
                    }
                    lcdCtrl.setTextColor(BURNT_ORANGE);

                    lcdCtrl.setCursor(stoppedPos_X, statePos_Y);
                    PrintToLcdWithBuf(buf, 16, "STOPPED");
                    if(OS_FALSE == paused) {
                        drawPause(ILI9341_CYAN);
                        drawPlay(BURNT_ORANGE);
                        paused = OS_FALSE;
                    }
                }
                break;
            case PC_PLAY:
                if(prevState != state) {
                    BOOLEAN redraw = OS_TRUE;
                    lcdCtrl.setTextColor(ILI9341_BLACK);
                    switch((CONTROL)prevState) {
                    case PC_STOP:
                        lcdCtrl.setCursor(stoppedPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "STOPPED");
                        break;
                    case PC_PAUSE:
                        lcdCtrl.setCursor(pausedPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "PAUSED");
                        break;
                    case PC_FF:
                        lcdCtrl.setCursor(fastForwardingPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "FAST FORWARDING");
                        redraw = OS_FALSE;
                        break;
                    case PC_RWD:
                        lcdCtrl.setCursor(rewindingPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "REWINDING");
                        redraw = OS_FALSE;
                        break;
                    default:
                        break;
                    }
                    lcdCtrl.setTextColor(BURNT_ORANGE);

                    lcdCtrl.setCursor(playingPos_X, statePos_Y);
                    PrintToLcdWithBuf(buf, 16, "PLAYING");
                    if(OS_TRUE == redraw) {
                        drawPlay(ILI9341_CYAN);
                        drawPause(BURNT_ORANGE);
                        paused = OS_FALSE;
                    }
                }
                break;
            case PC_PAUSE:
                if(prevState != state) {
                    BOOLEAN redraw = OS_TRUE;
                    lcdCtrl.setTextColor(ILI9341_BLACK);
                    switch((CONTROL)prevState) {
                    case PC_STOP:
                        lcdCtrl.setCursor(stoppedPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "STOPPED");
                        break;
                    case PC_PLAY:
                        lcdCtrl.setCursor(playingPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "PLAYING");
                        break;
                    case PC_FF:
                        lcdCtrl.setCursor(fastForwardingPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "FAST FORWARDING");
                        redraw = OS_FALSE;
                        break;
                    case PC_RWD:
                        lcdCtrl.setCursor(rewindingPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "REWINDING");
                        redraw = OS_FALSE;
                        break;
                    default:
                        break;
                    }
                    lcdCtrl.setTextColor(BURNT_ORANGE);

                    lcdCtrl.setCursor(pausedPos_X, statePos_Y);
                    PrintToLcdWithBuf(buf, 16, "PAUSED");
                    if(OS_TRUE == redraw) {
                        drawPause(ILI9341_CYAN);
                        drawPlay(BURNT_ORANGE);
                        paused = OS_TRUE;
                    }
                }
                break;
            case PC_FF:
                if(prevState != state) {
                    lcdCtrl.setTextColor(ILI9341_BLACK);
                    switch((CONTROL)prevState) {
                    case PC_STOP:
                        lcdCtrl.setCursor(stoppedPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "STOPPED");
                        break;
                    case PC_PLAY:
                        lcdCtrl.setCursor(playingPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "PLAYING");
                        break;
                    case PC_PAUSE:
                        lcdCtrl.setCursor(pausedPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "PAUSED");
                        break;
                    default:
                        break;
                    }
                    lcdCtrl.setTextColor(BURNT_ORANGE);

                    lcdCtrl.setCursor(fastForwardingPos_X, statePos_Y);
                    PrintToLcdWithBuf(buf, 16, "FAST FORWARDING");
                }
                break;
            case PC_RWD:
                if(prevState != state) {
                    lcdCtrl.setTextColor(ILI9341_BLACK);
                    switch((CONTROL)prevState) {
                    case PC_STOP:
                        lcdCtrl.setCursor(stoppedPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "STOPPED");
                        break;
                    case PC_PLAY:
                        lcdCtrl.setCursor(playingPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "PLAYING");
                        break;
                    case PC_PAUSE:
                        lcdCtrl.setCursor(pausedPos_X, statePos_Y);
                        PrintToLcdWithBuf(buf, 16, "PAUSED");
                        break;
                    default:
                        break;
                    }
                    lcdCtrl.setTextColor(BURNT_ORANGE);

                    lcdCtrl.setCursor(rewindingPos_X, statePos_Y);
                    PrintToLcdWithBuf(buf, 16, "REWINDING");
                }
                break;
            default:
                break;
            }

            switch(control) {
            case PC_VOLUP:
                updateVolumeSlider(OS_TRUE);
                break;
            case PC_VOLDOWN:
                updateVolumeSlider(OS_FALSE);
                break;
            case PC_NONE:
            case PC_SKIP:
            case PC_RESTART:
            default:
                break;
            }
            control = PC_NONE;
        }

        const float* progress = (const float*)OSMboxAccept(progressMessage);
        if(NULL != progress) {
            if(PC_STOP == prevState && prevState != state) {
                lcdCtrl.drawFastVLine(innerProgress_square.p0.p, innerProgress_square.p1.p,
                                      innerProgress_square.h, BURNT_ORANGE);
            }
            updateProgressBar(*progress, state);
        }

        prevState = state;

        const char* trackName = (const char*)OSMboxAccept(stream2LcdHandler);
        if(NULL != trackName) {
            updateTrackName(prevTrackName, trackName);
            memcpy(prevTrackName, trackName, maxTrackNameSize);
        }

        //Update other things when stopping before erasing lines.
        if(1 == stopped) {
            const uint8_t adjustedCount = 4 * lastCount;
            //Split erasing the lines to keep the flow of music data steady
            if(0 == flipFlop) {
                //Erase the lower left progress bar
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount),     5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount - 1), 5, ILI9341_BLACK);
                //Erase the lower right progress bar
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount),     5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount - 1), 5, ILI9341_BLACK);
                //Erase the upper left progress bar
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount),     5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount - 1), 5, ILI9341_BLACK);
                //Erase the upper right progress bar
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount),     5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount - 1), 5, ILI9341_BLACK);
                flipFlop = 1;
            }
            else {
                //Erase the lower left progress bar
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount - 2), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount - 3), 5, ILI9341_BLACK);
                //Erase the lower right progress bar
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount - 2), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount - 3), 5, ILI9341_BLACK);
                //Erase the upper left progress bar
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount - 2), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount - 3), 5, ILI9341_BLACK);
                //Erase the upper right progress bar
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount - 2), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount - 3), 5, ILI9341_BLACK);
                --lastCount;
                flipFlop = 0;
            }

            if(0 == lastCount) stopped = 0;
        }

        OSTimeDly(10);
    }
}