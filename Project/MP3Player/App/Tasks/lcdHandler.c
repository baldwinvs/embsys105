#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "Buttons.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>

#ifndef BUFSIZE
#define BUFSIZE 256
#endif

Adafruit_ILI9341 lcdCtrl = Adafruit_ILI9341(); // The LCD controller

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

//    lcdCtrl.drawRoundRect(play.x, play.y, play.w, play.h, 3, ILI9341_RED);
//    lcdCtrl.drawRoundRect(stop.x, stop.y, stop.w, stop.h, 3, ILI9341_GREEN);
//    // Print a message on the LCD
//    lcdCtrl.setCursor(play.x + 10, play.y + 10);
//    lcdCtrl.setTextColor(ILI9341_WHITE);
//    lcdCtrl.setTextSize(2);
//    PrintToLcdWithBuf(buf, BUFSIZE, "PLAY");
//
//    lcdCtrl.setCursor(stop.x + 10, stop.y + 10);
//    lcdCtrl.setTextColor(ILI9341_WHITE);
//    lcdCtrl.setTextSize(2);
//    PrintToLcdWithBuf(buf, BUFSIZE, "STOP");

    size_t i;
    for(i = 0; i < btn_array_sz; ++i) {
        SQUARE_BUTTON* btn = (SQUARE_BUTTON*)&btn_array[i];
        lcdCtrl.drawRoundRect(btn->x, btn->y, btn->w, btn->h, 3, ILI9341_ORANGE);
        lcdCtrl.setCursor(btn->x + 10, btn->y + 10);
        lcdCtrl.setTextColor(ILI9341_WHITE);
        lcdCtrl.setTextSize(2);
        PrintToLcdWithBuf(buf, BUFSIZE, btn->primaryName);
    }
}

void LcdHandlerTask(void* pData)
{
    PjdfErrCode pjdfErr;
    INT32U length;
    char buf[BUFSIZE];
    PrintWithBuf(buf, BUFSIZE, "LcdHandlerTask: starting\n");

	PrintWithBuf(buf, BUFSIZE, "Opening LCD driver: %s\n", PJDF_DEVICE_ID_LCD_ILI9341);
    // Open handle to the LCD driver
    HANDLE hLcd = Open(PJDF_DEVICE_ID_LCD_ILI9341, 0);
    if (!PJDF_IS_VALID_HANDLE(hLcd)) while(1);

	PrintWithBuf(buf, BUFSIZE, "Opening LCD SPI driver: %s\n", LCD_SPI_DEVICE_ID);
    // We talk to the LCD controller over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the LCD driver.
    HANDLE hSPI = Open(LCD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hLcd, PJDF_CTRL_LCD_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);

	PrintWithBuf(buf, BUFSIZE, "Initializing LCD controller\n");
    lcdCtrl.setPjdfHandle(hLcd);
    lcdCtrl.begin();

    lcdCtrl.setRotation(0);

    DrawLcdContents();

    while(1) {
        OSTimeDly(100);
    }
}