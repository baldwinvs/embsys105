#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "Buttons.h"
#include "InputCommands.h"
#include "PlayerCommand.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>

#define BURNT_ORANGE        (0xCAA0)    /* 255, 165,   0 */
#define DARK_BURNT_ORANGE   (0x7980)    /* 122,  85,   0 */

//Globals
extern OS_FLAG_GRP *initFlags;
extern OS_EVENT * touch2LcdHandler;
extern OS_EVENT * cmdHandler2LcdHandler;
extern OS_EVENT * stream2LcdHandler;
extern OS_EVENT * progressMessage;

/** @addtogroup init_bits
 * @{
 */
const uint32_t lcdHandlerEventBit       = 0x8;  //!< Event bit for the LcdHandlerTask.
/** @} */

Adafruit_ILI9341 lcdCtrl = Adafruit_ILI9341(); // The LCD controller

//! The Y position of the state string on the touch screen.
static const int16_t statePos_Y = 146;
//! The X position of the string "INITIALIZING" on the touch screen.
//! Calculated as: 120 - 6 * strlen("INITIALIZATING")
static const int16_t initializingPos_X = (120 - 60 - 12);
//! The X position of the string "INITIALIZED" on the touch screen.
//! Calculated as: 120 - 6 * strlen("INITIALIZED")
static const int16_t initializedPos_X = (120 - 55 - 11);
//! The X position of the string "PLAYING" on the touch screen.
//! Calculated as: 120 - 6 * strlen("PLAYING")
static const int16_t playingPos_X = (120 - 35 - 7);
//! The X position of the string "STOPPED" on the touch screen.
//! Calculated as: 120 - 6 * strlen("STOPPED")
static const int16_t stoppedPos_X = (120 - 35 - 7);
//! The X position of the string "PAUSED" on the touch screen.
//! Calculated as: 120 - 6 * strlen("PAUSED")
static const int16_t pausedPos_X  = (120 - 30 - 6);
//! The X position of the string "FAST FORWARDING" on the touch screen.
//! Calculated as: 120 - 6 * strlen("FAST FORWARDING")
static const int16_t fastForwardingPos_X = (120 - 75 - 15);
//! The X position of the string "REWINDING" on the touch screen.
//! Calculated as: 120 - 6 * strlen("REWINDING")
static const int16_t rewindingPos_X = (120 - 45 - 9);

//! The last received count value for the stop progress; range: [0, 40].
static uint8_t lastCount = 0;
//! Flag used for drawing the progress lines in reverse order.
//! **OS_TRUE** if receiving stop progress value of 0, **OS_FALSE** otherwise.
static BOOLEAN stopped = OS_FALSE;
//! Character array for writing strings to the GUI.
static char buf[SONGLEN];
//! The Y position of the *first* line of the song title.
static const int16_t line1Pos_Y = 55;
//! The Y position of the *second* line of the song title.
static const int16_t line2Pos_Y = line1Pos_Y + 18;

//! The increments of the volume slider on the volume bar.
static uint8_t sliderSpacing = 16;
//! The maximum value of the left most pixel line of the volume slider.
static const uint8_t maxSliderPosition = 196;
//! Value for the currently displayed volume.
static uint8_t volume = 4;  // range: [0-10], each value represents 10%

//! Scale factor for the song's progress value.
static const float progressScaleFactor = progress_icon.p2.p / 100.f;
/** The previous progress value after scaling.
 *
 * Useful for:
 * - Detecting if a song is progress forwards or in reverse
 * - Knowing how many pixel lines to fill in (when given gaps)
 */
static uint8_t previousProgress = 0;

//! Struct defining an (x, y) coordinate for the touch screen.
typedef struct point
{
    uint16_t x; //!< The x offset of the point.
    uint16_t y; //!< The y offset of the point.
} POINT;

//! Touch screen corners: lower left, lower right, top left, top right.
static const POINT corner[] = {{0, 320}, {235, 320}, {0, 0}, {235, 0}};

/** @brief Renders a character at the current cursor position on the LCD.
 *
 * @param c The character to be displayed.
 */
static void PrintCharToLcd(char c)
{
    lcdCtrl.write(c);
}

/** @brief Print a formatted string with the given buffer to the LCD.
 *
 * @param buf The buffer to write to.
 * @param size The number of bytes to write.
 * @param format The printf style formatting of the string.
 */
void PrintToLcdWithBuf(char *buf, int size, char *format, ...)
{
    va_list args;
    va_start(args, format);
    PrintToDeviceWithBuf(PrintCharToLcd, buf, size, format, args);
    va_end(args);
}

/** @brief Draw a filled button.
 *
 * \note The radius parameter is only used by square buttons.
 *
 * @param btn The BTN struct defining the button.
 * @param color The color of the fill of the button.
 * @param radius The radius of the rounded portions of a rounded rectangle.
 */
static void drawButton(const BTN btn, const uint16_t color, const uint16_t radius);

/** @brief Draw the contents of the GUI.
 */
static void drawLcdContents(void);

/** Draw the pause icon.
 *
 * @param color The color of the filled icon.
 */
static void drawPause(const uint16_t color);

/** @brief Draw the play icon.
 *
 * @param color The color of the filled icon.
 */
static void drawPlay(const uint16_t color);

/** @brief Handles drawing over the volume slider and filling in the volume bar where the
 * slider was located; also increment (by subtracting 1, @see updateVolumeSlider()) volume slider
 * before it is redrawn.
 */
static void incrementVolumeSlider(void);

/** @brief Handles drawing over the volume slider and filling in the volume bar where the
 * slider was located; also decrement (by adding 1, @see updateVolumeSlider()) volume slider
 * before it is redrawn.
 */
static void decrementVolumeSlider(void);

/** @brief Update the volume slider based on the boolean input.
 *
 * @param volumeUp **OS_TRUE** results in an increase in volume.
 */
static void updateVolumeSlider(const BOOLEAN volumeUp);

/** @brief Helper function for updating the state of the GUI.
 *
 * @param prev The previous state.
 * @param curr The current state.
 */
static void updateStateHelper(const COMMAND prev, const COMMAND curr);

/** @brief Update the state of the GUI when given a command message.
 *
 * @param commandMsg The command message containing the updated state and current command.
 */
static void updateState(const COMMAND commandMsg);

/** @brief Update the progress bar based on progress value that is input.
 *
 * @param progress The progress value with range [0.0, 100.0]
 */
static void updateProgressBar(const float progress);

/** @brief Remove the previous track name and draw the new track name.
 *
 * @param newTrack The new track string.
 */
static void updateTrackName(const char* newTrack);

/** @brief Draw the lines for the stop progress indicator.
 *
 * @param stopProgress The stop progress value; range [0, 40]
 */
static void drawStopProgressLines(const uint16_t stopProgress);

/** @brief Draw over the previously drawn progress lines at the rate of 2 lines
 * at each corner for each frame in which the user is not transmitting a stop
 * progress value; this will give a smooth animation and the music will not be
 * choppy.
 */
static void eraseStopProgressLines(void);

void LcdHandlerTask(void* pData)
{
    PjdfErrCode pjdfErr;
    uint32_t length;

    // Open handle to the LCD driver
    HANDLE hLcd = Open(PJDF_DEVICE_ID_LCD_ILI9341, 0);
    if (!PJDF_IS_VALID_HANDLE(hLcd)) {
        PrintFormattedString("Failure opening LCD driver: %s\n", PJDF_DEVICE_ID_LCD_ILI9341);
        while(1);
    }

    // We talk to the LCD controller over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the LCD driver.
    HANDLE hSPI = Open(LCD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) {
        PrintFormattedString("Failure opening LCD SPI driver: %s\n", LCD_SPI_DEVICE_ID);
        while(1);
    }

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hLcd, PJDF_CTRL_LCD_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);

    lcdCtrl.setPjdfHandle(hLcd);
    lcdCtrl.begin();

    lcdCtrl.setRotation(0);

    // Initialize the GUI.
    drawLcdContents();

    // Initialization complete, set the event bit and wait for other tasks to complete before proceeding.
    {
        INT8U err;
        OSFlagPost(initFlags, lcdHandlerEventBit, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("LcdHandlerTask: posting to flag group with error code %d\n", (uint32_t)err);
            while(1);
        }

        OSFlagPend(initFlags, lcdHandlerEventBit, OS_FLAG_WAIT_CLR_ALL, 0, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("LcdHandlerTask: pending on flag group (bit 0x8) with error code %d\n", (uint32_t)err);
            while(1);
        }
    }

    // Now initialized, draw over the previous state string.
    lcdCtrl.setTextColor(ILI9341_BLACK);
    lcdCtrl.setCursor(initializingPos_X, statePos_Y);
    PrintToLcdWithBuf(buf, 16, "INITIALIZING");

    // Update state to INITIALIZED.
    lcdCtrl.setTextColor(BURNT_ORANGE);
    lcdCtrl.setCursor(initializedPos_X, statePos_Y);
    PrintToLcdWithBuf(buf, 16, "INITIALIZED");

    // Not necessary, but it's a nice feeling to have a short delay to see that it
    // has actually initialized.
    OSTimeDly(OS_TICKS_PER_SEC);

    // Draw over the previous state string.
    lcdCtrl.setTextColor(ILI9341_BLACK);
    lcdCtrl.setCursor(initializedPos_X, statePos_Y);
    PrintToLcdWithBuf(buf, 16, "INITIALIZED");

    // Finally, draw the initial state.
    lcdCtrl.setTextColor(BURNT_ORANGE);
    lcdCtrl.setCursor(stoppedPos_X, statePos_Y);
    PrintToLcdWithBuf(buf, 16, "STOPPED");

    uint32_t* msgReceived = NULL;
    const float* songProgress = NULL;
    const char* trackName = NULL;

    while(OS_TRUE) {
        msgReceived = (uint32_t*)OSMboxAccept(touch2LcdHandler);
        if(NULL != msgReceived) {
            drawStopProgressLines(*(uint16_t*)msgReceived);
        }

        msgReceived = (uint32_t*)OSMboxAccept(cmdHandler2LcdHandler);
        if(NULL != msgReceived) {
            updateState(*(COMMAND *)msgReceived);
        }

        songProgress = (const float*)OSMboxAccept(progressMessage);
        if(NULL != songProgress) {
            updateProgressBar(*songProgress);
        }

        trackName = (const char*)OSMboxAccept(stream2LcdHandler);
        if(NULL != trackName) {
            updateTrackName(trackName);
        }

        eraseStopProgressLines();

        // 10 ticks is a bit arbitrary, it works well at this value.
        OSTimeDly(10);
    }
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

static void drawLcdContents(void)
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
    lcdCtrl.setCursor(vol_down_btn.p0.p + vol_down_btn.p2.p/4, vol_down_btn.p1.p + 6);
    PrintToLcdWithBuf(buf, 12, (char*)volume_down);

    drawButton(vol_bar, ILI9341_CYAN, 2);
    drawButton(vol_slider, BURNT_ORANGE, 2);

    //don't draw volume up rectangle, do draw text
    lcdCtrl.setCursor(vol_up_btn.p0.p + vol_up_btn.p2.p/4, vol_up_btn.p1.p + 6);
    PrintToLcdWithBuf(buf, 12, (char*)volume_up);

    drawButton(outer_progress_icon, ILI9341_CYAN, 4);
    drawButton(progress_icon, ILI9341_BLACK, 0);

    drawButton(restart_btn, BURNT_ORANGE, 5);
    const BTN restart_btn_inner = {S_SQUARE, restart_btn.p0.p+2, restart_btn.p1.p+2, restart_btn.p2.p-4, restart_btn.h-4};
    drawButton(restart_btn_inner, ILI9341_CYAN, 5);
    drawButton(skip_btn, BURNT_ORANGE, 5);
    const BTN skip_btn_inner = {S_SQUARE, skip_btn.p0.p+2, skip_btn.p1.p+2, skip_btn.p2.p-4, skip_btn.h-4};
    drawButton(skip_btn_inner, ILI9341_CYAN, 5);

    drawButton(play_btn, BURNT_ORANGE, 0);
    const BTN play_btn_inner = {S_CIRCLE, play_btn.p0.p, play_btn.p1.p, play_btn.p2.p-3, 0};
    drawButton(play_btn_inner, ILI9341_CYAN, 0);

    drawButton(play_icon, BURNT_ORANGE, 0);

    drawButton(restart_left_icon, BURNT_ORANGE, 0);
    drawButton(restart_right_icon, BURNT_ORANGE, 0);
    drawButton(restart_end_icon, BURNT_ORANGE, 0);

    drawButton(skip_left_icon, BURNT_ORANGE, 0);
    drawButton(skip_right_icon, BURNT_ORANGE, 0);
    drawButton(skip_end_icon, BURNT_ORANGE, 0);

    lcdCtrl.setTextSize(2);
}

static void drawPause(const uint16_t color)
{
    drawButton(pause_left_icon, color, 1);
    drawButton(pause_right_icon, color, 1);
}

static void drawPlay(const uint16_t color)
{
    drawButton(play_icon, color, 0);
}

static void incrementVolumeSlider(void)
{
    // Don't increment if at 100% volume.
    if(0 == volume) return;

    // Fill in the previous slider position to give the sense of it disappearing.
    drawButton(vol_slider, ILI9341_BLACK, 2);
    if(10 == volume) {
        // Get the small section of the volume bar that was drawn over and redraw as original color
        // to the sense that it was always there.
        const BTN vol_bar_segment = {S_SQUARE, vol_slider.p0.p, vol_bar.p1.p, vol_slider.p2.p, vol_bar.h};
        drawButton(vol_bar_segment, ILI9341_CYAN, 2);
        for(size_t i = vol_bar.p1.p; i < vol_bar.p1.p + vol_bar.h; ++i) {
            lcdCtrl.drawFastHLine(vol_slider.p0.p + 2, i, vol_slider.p2.p - 2, ILI9341_CYAN);
        }
    }
    else if(volume > 0) {
        // Redraw the volume bar that was drawn over.
        for(size_t i = vol_bar.p1.p; i < vol_bar.p1.p + vol_bar.h; ++i) {
            lcdCtrl.drawFastHLine(vol_slider.p0.p, i, vol_slider.p2.p, ILI9341_CYAN);
        }
    }
    --volume;
}

static void decrementVolumeSlider(void)
{
    // Don't decrement if at 0% volume.
    if(10 == volume) return;

    // Fill in the previous slider position to give the sense of it disappearing.
    drawButton(vol_slider, ILI9341_BLACK, 2);
    if(0 == volume) {
        // Get the small section of the volume bar that was drawn over and redraw as original color
        // to the sense that it was always there.
        const BTN vol_bar_segment = {S_SQUARE, vol_slider.p0.p, vol_bar.p1.p, vol_slider.p2.p, vol_bar.h};
        drawButton(vol_bar_segment, ILI9341_CYAN, 2);
        for(size_t i = vol_bar.p1.p; i < vol_bar.p1.p + vol_bar.h; ++i) {
            lcdCtrl.drawFastHLine(vol_slider.p0.p, i, vol_slider.p2.p - 2, ILI9341_CYAN);
        }
    }
    else if(volume < 10) {
        // Redraw the volume bar that was drawn over.
        for(size_t i = vol_bar.p1.p; i < vol_bar.p1.p + vol_bar.h; ++i) {
            lcdCtrl.drawFastHLine(vol_slider.p0.p, i, vol_slider.p2.p, ILI9341_CYAN);
        }
    }
    ++volume;
}

static void updateVolumeSlider(const BOOLEAN volumeUp)
{
    if(OS_TRUE == volumeUp) {
        incrementVolumeSlider();
    }
    else {
        decrementVolumeSlider();
    }
    // Calculate the new slider position and redraw.
    vol_slider.p0.p = maxSliderPosition - (sliderSpacing * volume);
    drawButton(vol_slider, BURNT_ORANGE, 2);
}

static void updateStateHelper(const COMMAND prev, const COMMAND curr)
{
        static BOOLEAN paused = OS_FALSE;
        BOOLEAN redraw = OS_TRUE;
        char prevStateString[16] = {0};
        char stateString[16] = {0};
        int16_t prevPosition_X = 0;
        int16_t currPosition_X = 0;

        lcdCtrl.setTextColor(ILI9341_BLACK);

        // Determine the previous state's X offset and string.
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

        // Determine the current state's X offset and string.
        switch(curr) {
        case PC_STOP:
            memcpy(stateString, "STOPPED", 16);
            currPosition_X = stoppedPos_X;
            if(OS_FALSE == paused) {
                // Show the play icon.
                drawPause(ILI9341_CYAN);
                drawPlay(BURNT_ORANGE);
            }
            break;
        case PC_PLAY:
            memcpy(stateString, "PLAYING", 16);
            currPosition_X = playingPos_X;
            if(OS_TRUE == redraw) {
                // Show the play icon.
                drawPlay(ILI9341_CYAN);
                drawPause(BURNT_ORANGE);
                paused = OS_FALSE;
            }
            break;
        case PC_PAUSE:
            memcpy(stateString, "PAUSED", 16);
            currPosition_X = pausedPos_X;
            if(OS_TRUE == redraw) {
                // Show the pause icon.
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
        // Draw over the previous state.
        lcdCtrl.setCursor(prevPosition_X, statePos_Y);
        PrintToLcdWithBuf(buf, 16, "%s", prevStateString);

        // Then draw the current state.
        lcdCtrl.setTextColor(BURNT_ORANGE);
        lcdCtrl.setCursor(currPosition_X, statePos_Y);
        PrintToLcdWithBuf(buf, 16, "%s", stateString);
}

static void updateState(const COMMAND commandMsg)
{
    static COMMAND prevState = PC_STOP;
    // Extract the state and control values.
    const COMMAND state = (COMMAND)(commandMsg & stateMask);
    const COMMAND control = (COMMAND)(commandMsg & controlMask);

    // Update the GUI if the state has changed.
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

    // Update the GUI if the volume has changed.
    // Changes to the song via skip/restart are reflected when StreamingTask
    //   sends the updated track information.
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
    // The progress value is in the range of [0.0, 100.0].
    // The scaledProgress is in the range of [progress_icon.po.p, progress_icon.p2.p]
    //   therefore the progress value must be scaled to accomodate this.
    const uint8_t scaledProgress = (uint8_t)(progress * progressScaleFactor);

    // Occurs in states: PLAY, FAST FORWARDING.
    if(scaledProgress > previousProgress) {
        // Draw vertical lines for range [previousProgress, scaledProgress)
        do {
            lcdCtrl.drawFastVLine(progress_icon.p0.p + previousProgress, progress_icon.p1.p,
                                  progress_icon.h, BURNT_ORANGE);
        }
        while(++previousProgress < scaledProgress);
        // Then draw the line at scaledProgress.
        lcdCtrl.drawFastVLine(progress_icon.p0.p + scaledProgress, progress_icon.p1.p,
                              progress_icon.h, BURNT_ORANGE);
    }
    // Occurs in state: REWINDING.
    else if (scaledProgress < previousProgress) {
        // Draw over all progress lines currently drawn to reset progress.
        if(scaledProgress == 0) {
            drawButton(progress_icon, ILI9341_BLACK, 0);
            previousProgress = scaledProgress;
        }
        else {
            // Draw vertical lines for range (scaledProgress, previousProgress].
            do {
                lcdCtrl.drawFastVLine(progress_icon.p0.p + previousProgress, progress_icon.p1.p,
                                      progress_icon.h, ILI9341_BLACK);
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

    const uint8_t length = (uint8_t)strlen(newTrack);

    // Draw over the previous track name.
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

    // Determine if the track name will fit on 1 line or 2 lines.
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

    // Draw the new track name.
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

static void drawStopProgressLines(const uint16_t stopProgress)
{
    // 0xYYZZ, YY is max count, ZZ is current count
    const uint8_t max = (uint8_t)((stopProgress >> 8) & 0x00FF);
    const uint8_t count = (uint8_t)(stopProgress & 0x00FF);

    // When count is in range [1, 40].
    if(count % (max + 1) != 0) {
        const uint8_t adjustedCount = 4 * count;
        // For each corner, draw 4 lines based on the value of the adjustedCount.
        for(uint8_t i = 0; i < 2; ++i) {
            for(int8_t j = 3; j >= 0; --j) {
                lcdCtrl.drawFastHLine(corner[i].x, corner[i].y - (adjustedCount - j), 5, BURNT_ORANGE);
            }
        }
        for(uint8_t i = 0; i < 2; ++i) {
            for(int8_t j = 3; j >= 0; --j) {
                lcdCtrl.drawFastHLine(corner[i + 2].x, corner[i + 2].y + (adjustedCount - j), 5, BURNT_ORANGE);
            }
        }
        lastCount = count;
    }
    // When count is zero, the user has released the button and sent the last message.
    else {
        stopped = OS_TRUE;
    }
}

static void eraseStopProgressLines(void)
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
