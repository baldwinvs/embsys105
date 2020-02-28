#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "Buttons.h"
#include "PlayerControl.h"

//Globals
extern OS_FLAG_GRP *rxFlags;       // Event flags for synchronizing mailbox messages
extern OS_EVENT * touch2LcdHandler;
extern OS_EVENT * cmdHandler2LcdHandler;

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>

Adafruit_ILI9341 lcdCtrl = Adafruit_ILI9341(); // The LCD controller

#define BLUE                (0x4398)    /*  68, 114, 196 */
#define BURNT_ORANGE        (0xCAA0)    /* 255, 165,   0 */
#define DARK_BURNT_ORANGE   (0x7980)    /* 122,  85,   0 */

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

static void DrawLcdContents()
{
	char buf[BUFSIZE];
    lcdCtrl.fillScreen(ILI9341_BLACK);

    lcdCtrl.setTextColor(BURNT_ORANGE);
    lcdCtrl.setTextSize(4);
    //don't draw volume down rectangle, do draw text
    lcdCtrl.setCursor(vol_dwn.x + vol_dwn.w/4, vol_dwn.y + 6);
    PrintToLcdWithBuf(buf, BUFSIZE, (char*)vol_dwn.name);

    lcdCtrl.fillRect(vol_bar.x, vol_bar.y, vol_bar.w, vol_bar.h, BLUE);

    //don't draw volume up rectangle, do draw text
    lcdCtrl.setCursor(vol_up.x + vol_up.w/4, vol_up.y + 6);
    PrintToLcdWithBuf(buf, BUFSIZE, (char*)vol_up.name);

    lcdCtrl.fillRoundRect(restart_square.x, restart_square.y, restart_square.w, restart_square.h, 5, DARK_BURNT_ORANGE);
    lcdCtrl.fillRoundRect(restart_square.x+2, restart_square.y+2, restart_square.w-4, restart_square.h-4, 5, ILI9341_CYAN);
    lcdCtrl.fillRoundRect(skip_square.x, skip_square.y, skip_square.w, skip_square.h, 5, DARK_BURNT_ORANGE);
    lcdCtrl.fillRoundRect(skip_square.x+2, skip_square.y+2, skip_square.w-4, skip_square.h-4, 5, ILI9341_CYAN);

    lcdCtrl.fillCircle(play_circle.x, play_circle.y, play_circle.r, BURNT_ORANGE);
    lcdCtrl.fillCircle(play_circle.x, play_circle.y, play_circle.r-3, BLUE);

    lcdCtrl.fillTriangle(play_icon.x0, play_icon.y0, play_icon.x1, play_icon.y1, play_icon.x2, play_icon.y2, BURNT_ORANGE);
}

void LcdHandlerTask(void* pData)
{
    PjdfErrCode pjdfErr;
    INT32U length;
    PrintFormattedString("LcdHandlerTask: starting\n");

	PrintFormattedString("Opening LCD driver: %s\n", PJDF_DEVICE_ID_LCD_ILI9341);
    // Open handle to the LCD driver
    HANDLE hLcd = Open(PJDF_DEVICE_ID_LCD_ILI9341, 0);
    if (!PJDF_IS_VALID_HANDLE(hLcd)) while(1);

	PrintFormattedString("Opening LCD SPI driver: %s\n", LCD_SPI_DEVICE_ID);
    // We talk to the LCD controller over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the LCD driver.
    HANDLE hSPI = Open(LCD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hLcd, PJDF_CTRL_LCD_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);

	PrintFormattedString("Initializing LCD controller\n");
    lcdCtrl.setPjdfHandle(hLcd);
    lcdCtrl.begin();

    lcdCtrl.setRotation(0);

    DrawLcdContents();

    {
        INT8U err;
        OSFlagPost(rxFlags, 8, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("LcdHandlerTask: posting to flag group with error code %d\n", (INT32U)err);
        }
    }
    uint32_t* msgReceived = NULL;
    uint8_t stopped = 0;
    uint8_t lastCount = 0;
    uint8_t paused = 0;
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
            switch(*(CONTROL *)msgReceived) {
            case PC_STOP:
                if(OS_FALSE == paused) {
                    lcdCtrl.fillRoundRect(pause_left_icon.x, pause_left_icon.y,
                                          pause_left_icon.w, pause_left_icon.h,
                                          1, BLUE);
                    lcdCtrl.fillRoundRect(pause_right_icon.x, pause_right_icon.y,
                                          pause_right_icon.w, pause_right_icon.h,
                                          1, BLUE);
                    lcdCtrl.fillTriangle(play_icon.x0, play_icon.y0,
                                         play_icon.x1, play_icon.y1,
                                         play_icon.x2, play_icon.y2,
                                         BURNT_ORANGE);
                    paused = OS_FALSE;
                }
                break;
            case PC_PLAY:
                lcdCtrl.fillTriangle(play_icon.x0, play_icon.y0,
                                     play_icon.x1, play_icon.y1,
                                     play_icon.x2, play_icon.y2,
                                     BLUE);
                lcdCtrl.fillRoundRect(pause_left_icon.x, pause_left_icon.y,
                                      pause_left_icon.w, pause_left_icon.h,
                                      1, BURNT_ORANGE);
                lcdCtrl.fillRoundRect(pause_right_icon.x, pause_right_icon.y,
                                      pause_right_icon.w, pause_right_icon.h,
                                      1, BURNT_ORANGE);
                paused = OS_FALSE;
                break;
            case PC_PAUSE:
                lcdCtrl.fillRoundRect(pause_left_icon.x, pause_left_icon.y,
                                      pause_left_icon.w, pause_left_icon.h,
                                      1, BLUE);
                lcdCtrl.fillRoundRect(pause_right_icon.x, pause_right_icon.y,
                                      pause_right_icon.w, pause_right_icon.h,
                                      1, BLUE);
                lcdCtrl.fillTriangle(play_icon.x0, play_icon.y0,
                                     play_icon.x1, play_icon.y1,
                                     play_icon.x2, play_icon.y2,
                                     BURNT_ORANGE);
                paused = OS_TRUE;
                break;
            default:
                break;
            }
        }

        //Update other things when stopping before erasing lines.
        if(1 == stopped) {
            while(lastCount) {
                const uint8_t adjustedCount = 4 * lastCount;
                //Erase the lower left progress bar
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount),     5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount - 1), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount - 2), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(0, 320 - (adjustedCount - 3), 5, ILI9341_BLACK);
                //Erase the lower right progress bar
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount),     5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount - 1), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount - 2), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(235, 320 - (adjustedCount - 3), 5, ILI9341_BLACK);
                //Erase the upper left progress bar
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount),     5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount - 1), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount - 2), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(0, 0 + (adjustedCount - 3), 5, ILI9341_BLACK);
                //Erase the upper right progress bar
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount),     5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount - 1), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount - 2), 5, ILI9341_BLACK);
                lcdCtrl.drawFastHLine(235, 0 + (adjustedCount - 3), 5, ILI9341_BLACK);
                --lastCount;
            }
            stopped = 0;
        }

        OSTimeDly(10);
    }
}