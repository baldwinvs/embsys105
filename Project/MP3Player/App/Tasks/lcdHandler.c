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

static const int16_t statePos_Y = 146;
static const int16_t initializingPos_X = (120 - 60 - 12);    // center x pos [120] - (5 * char count) - char count
static const int16_t initializedPos_X = (120 - 55 - 11);     // center x pos [120] - (5 * char count)
static const int16_t playingPos_X = (120 - 35 - 7);         // center x pos [120] - (5 * char count)
static const int16_t stoppedPos_X = (120 - 35 - 7);         // center x pos [120] - (5 * char count)
static const int16_t pausedPos_X  = (120 - 30 - 6);         // center x pos [120] - (5 * char count)
static const int16_t fastForwardingPos_X = (120 - 75 - 15);  // center x pos [120] - (5 * char count)
static const int16_t rewindingPos_X = (120 - 45 - 9);       // center x pos [120] - (5 * char count)

static uint8_t lastCount = 0;
static BOOLEAN stopped = OS_FALSE;
static char buf[SONGLEN];
static const int16_t line1Pos_Y = 50;
static const int16_t line2Pos_Y = line1Pos_Y + 18;  // line 1 + 18

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

    drawButton(vol_bar, ILI9341_CYAN, 2);
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
static void drawPlay(const uint16_t color)
{
    drawButton(play_triangle, color, 0);
}

static void incrementVolumeSlider(void)
{
    if(0 == volume) return;

    //fill in the previous slider position
    drawButton(vol_slider, ILI9341_BLACK, 2);
    if(10 == volume) {
        const BTN vol_bar_segment = {S_SQUARE, vol_slider.p0.p, vol_bar.p1.p, vol_slider.p2.p, vol_bar.h};
        drawButton(vol_bar_segment, ILI9341_CYAN, 2);
        for(size_t i = vol_bar.p1.p; i < vol_bar.p1.p + vol_bar.h; ++i) {
            lcdCtrl.drawFastHLine(vol_slider.p0.p + 2, i, vol_slider.p2.p - 2, ILI9341_CYAN);
        }
    }
    else if(volume > 0) {
        //redraw the volume bar that was drawn over
        for(size_t i = vol_bar.p1.p; i < vol_bar.p1.p + vol_bar.h; ++i) {
            lcdCtrl.drawFastHLine(vol_slider.p0.p, i, vol_slider.p2.p, ILI9341_CYAN);
        }
    }
    --volume;
}

static void decrementVolumeSlider(void)
{
    if(10 == volume) return;

    //fill in the previous slider position
    drawButton(vol_slider, ILI9341_BLACK, 2);
    if(0 == volume) {
        const BTN vol_bar_segment = {S_SQUARE, vol_slider.p0.p, vol_bar.p1.p, vol_slider.p2.p, vol_bar.h};
        drawButton(vol_bar_segment, ILI9341_CYAN, 2);
        for(size_t i = vol_bar.p1.p; i < vol_bar.p1.p + vol_bar.h; ++i) {
            lcdCtrl.drawFastHLine(vol_slider.p0.p, i, vol_slider.p2.p - 2, ILI9341_CYAN);
        }
    }
    else if(volume < 10) {
        //redraw the volume bar that was drawn over
        for(size_t i = vol_bar.p1.p; i < vol_bar.p1.p + vol_bar.h; ++i) {
            lcdCtrl.drawFastHLine(vol_slider.p0.p, i, vol_slider.p2.p, ILI9341_CYAN);
        }
    }
    ++volume;
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

static void updateStateHelper(const CONTROL prev, const CONTROL curr)
{
        static BOOLEAN paused = OS_FALSE;
        BOOLEAN redraw = OS_TRUE;
        char prevStateString[16] = {0};
        char stateString[16] = {0};
        int16_t prevPosition_X = 0;
        int16_t currPosition_X = 0;

        lcdCtrl.setTextColor(ILI9341_BLACK);

        switch(prev) {
        case PC_STOP:
            prevPosition_X = stoppedPos_X;
            memcpy(prevStateString, "STOPPED", 16);
            break;
        case PC_PLAY:
            prevPosition_X = playingPos_X;
            memcpy(prevStateString, "PLAYING", 16);
            break;
        case PC_PAUSE:
            prevPosition_X = pausedPos_X;
            memcpy(prevStateString, "PAUSED", 16);
            break;
        case PC_FF:
            prevPosition_X = fastForwardingPos_X;
            memcpy(prevStateString, "FAST FORWARDING", 16);
            redraw = OS_FALSE;
            break;
        case PC_RWD:
            prevPosition_X = rewindingPos_X;
            memcpy(prevStateString, "REWINDING", 16);
            redraw = OS_FALSE;
            break;
        default:
            break;
        }

        switch(curr) {
        case PC_STOP:
            memcpy(stateString, "STOPPED", 16);
            currPosition_X = stoppedPos_X;
            if(OS_FALSE == paused) {
                drawPause(ILI9341_CYAN);
                drawPlay(BURNT_ORANGE);
            }
            break;
        case PC_PLAY:
            memcpy(stateString, "PLAYING", 16);
            currPosition_X = playingPos_X;
            if(OS_TRUE == redraw) {
                drawPlay(ILI9341_CYAN);
                drawPause(BURNT_ORANGE);
                paused = OS_FALSE;
            }
            break;
        case PC_PAUSE:
            memcpy(stateString, "PAUSED", 16);
            currPosition_X = pausedPos_X;
            if(OS_TRUE == redraw) {
                drawPause(ILI9341_CYAN);
                drawPlay(BURNT_ORANGE);
                paused = OS_TRUE;
            }
            break;
        case PC_FF:
            memcpy(stateString, "FAST FORWARDING", 16);
            currPosition_X = fastForwardingPos_X;
            break;
        case PC_RWD:
            memcpy(stateString, "REWINDING", 16);
            currPosition_X = rewindingPos_X;
            break;
        default:
            break;
        }
        lcdCtrl.setCursor(prevPosition_X, statePos_Y);
        PrintToLcdWithBuf(buf, 16, "%s", prevStateString);

        lcdCtrl.setTextColor(BURNT_ORANGE);
        lcdCtrl.setCursor(currPosition_X, statePos_Y);
        PrintToLcdWithBuf(buf, 16, "%s", stateString);
}

static void updateState(const CONTROL commandMsg)
{
    static CONTROL prevState = PC_STOP;
    const CONTROL state = (CONTROL)(commandMsg & stateMask);
    const CONTROL control = (CONTROL)(commandMsg & controlMask);

    switch(state) {
    case PC_STOP:
    case PC_PLAY:
    case PC_PAUSE:
    case PC_FF:
    case PC_RWD:
        if(prevState != state) {
            updateStateHelper(prevState, state);
        }
    default:
        break;
    }
    prevState = state;

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
}

static void updateProgressBar(const float progress)
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

static void updateTrackName(const char* newTrack)
{
    static const uint8_t maxCharsPerLine = 18;
    static BOOLEAN use2Lines = OS_FALSE;
    static char prevTrack[SONGLEN] = {0};
    static char prevTrackName_1[SONGLEN] = {0};
    static char prevTrackName_2[SONGLEN] = {0};
    static uint8_t prevPos_X1 = 0;
    static uint8_t prevPos_X2 = 0;

    lcdCtrl.setTextColor(ILI9341_BLACK);
    if(OS_TRUE == use2Lines) {
        const size_t s1Len = strlen(prevTrackName_1);
        const size_t s2Len = strlen(prevTrackName_2);

        prevPos_X1 = 122 - 6 * s1Len;
        prevPos_X2 = 122 - 6 * s2Len;
        lcdCtrl.setCursor(prevPos_X1, line1Pos_Y);
        PrintToLcdWithBuf(buf, maxCharsPerLine, prevTrackName_1);
        lcdCtrl.setCursor(prevPos_X2, line2Pos_Y);
        PrintToLcdWithBuf(buf, maxCharsPerLine, prevTrackName_2);
    }
    else {
        const size_t len = strlen(prevTrack);
        const int16_t pos = 120 - 6 * len;
        lcdCtrl.setCursor((pos < 5 ? 5 : pos), line1Pos_Y);
        PrintToLcdWithBuf(buf, SONGLEN, prevTrack);
    }

    const uint8_t length = (uint8_t)strlen(newTrack);
    if(length > maxCharsPerLine) {
        // look for a space, starting at the end
        uint8_t i;
        for(i = length; i > 0; --i) {
            if(0 == strncmp(" ", &newTrack[i - 1], 1)) {
                if(i > maxCharsPerLine) continue;
                else {
                    use2Lines = OS_TRUE;
                    memcpy(prevTrackName_1, newTrack, i - 1);
                    memcpy(prevTrackName_2, &newTrack[i], length - i);
                    break;
                }
            }
        }
        if(0 == i) use2Lines = OS_FALSE;
    }
    else use2Lines = OS_FALSE;

    lcdCtrl.setTextColor(BURNT_ORANGE);
    if(OS_TRUE == use2Lines) {
        const size_t s1Len = strlen(prevTrackName_1);
        const size_t s2Len = strlen(prevTrackName_2);

        prevPos_X1 = 122 - 6 * s1Len;
        prevPos_X2 = 122 - 6 * s2Len;
        lcdCtrl.setCursor(prevPos_X1, line1Pos_Y);
        PrintToLcdWithBuf(buf, maxCharsPerLine, prevTrackName_1);
        lcdCtrl.setCursor(prevPos_X2, line2Pos_Y);
        PrintToLcdWithBuf(buf, maxCharsPerLine, prevTrackName_2);
    }
    else {
        const size_t len = strlen(newTrack);
        const int16_t pos = 120 - 6 * len;
        lcdCtrl.setCursor((pos < 5 ? 5 : pos), line1Pos_Y);
        PrintToLcdWithBuf(buf, SONGLEN, (char*)newTrack);
        memcpy(prevTrack, newTrack, SONGLEN);
    }
}

void drawStopProgressLines(const uint16_t stopProgress)
{
    // 0xYYZZ, YY is max count, ZZ is current count
    const uint8_t max = (uint8_t)((stopProgress >> 8) & 0x00FF);
    const uint8_t count = (uint8_t)(stopProgress & 0x00FF);

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
        stopped = OS_TRUE;
    }
}

void eraseStopProgressLines()
{
    static BOOLEAN flipFlop = OS_FALSE;

    if(OS_TRUE == stopped) {
        const uint8_t adjustedCount = 4 * lastCount;
        //Split erasing the lines to keep the flow of music data steady
        if(OS_FALSE == flipFlop) {
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
            flipFlop = OS_TRUE;
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
            flipFlop = OS_FALSE;
        }

        if(0 == lastCount) stopped = OS_FALSE;
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
    const float* songProgress = NULL;
    const char* trackName = NULL;

    while(1) {
        msgReceived = (uint32_t*)OSMboxAccept(touch2LcdHandler);
        if(NULL != msgReceived) {
            drawStopProgressLines(*(uint16_t*)msgReceived);
        }

        msgReceived = (uint32_t*)OSMboxAccept(cmdHandler2LcdHandler);
        if(NULL != msgReceived) {
            updateState(*(CONTROL *)msgReceived);
        }

        songProgress = (const float*)OSMboxAccept(progressMessage);
        if(NULL != songProgress) {
            updateProgressBar(*songProgress);
        }

        trackName = (const char*)OSMboxAccept(stream2LcdHandler);
        if(NULL != trackName) {
            updateTrackName(trackName);
        }

        //Update other things when stopping before erasing lines.
        eraseStopProgressLines();

        OSTimeDly(10);
    }
}