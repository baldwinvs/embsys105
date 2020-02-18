#include "tasks.h"

#include "bsp.h"
#include "mp3Util.h"
#include "print.h"

//#include "train_crossing.h"
//#include "check_please.h"
//#include "dogen.h"
//#include "dogen_2.h"
#include "dogen1_2.h"

#ifndef BUFSIZE
#define BUFSIZE 256
#endif

/************************************************************************************

Runs MP3 demo code

************************************************************************************/
void StreamingTask(void* pData)
{
    PjdfErrCode pjdfErr;
    INT32U length;

    OSTimeDly(OS_TICKS_PER_SEC * 2); // Allow other task to initialize LCD before we use it.

	char buf[BUFSIZE];
	PrintWithBuf(buf, BUFSIZE, "StreamingTask: starting\n");

	PrintWithBuf(buf, BUFSIZE, "Opening MP3 driver: %s\n", PJDF_DEVICE_ID_MP3_VS1053);
    // Open handle to the MP3 decoder driver
    HANDLE hMp3 = Open(PJDF_DEVICE_ID_MP3_VS1053, 0);
    if (!PJDF_IS_VALID_HANDLE(hMp3)) while(1);

	PrintWithBuf(buf, BUFSIZE, "Opening MP3 SPI driver: %s\n", MP3_SPI_DEVICE_ID);
    // We talk to the MP3 decoder over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the MP3 driver.
    HANDLE hSPI = Open(MP3_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hMp3, PJDF_CTRL_MP3_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);

    // Initialize SD card
    PrintWithBuf(buf, PRINTBUFMAX, "Opening handle to SD driver: %s\n", PJDF_DEVICE_ID_SD_ADAFRUIT);
    HANDLE hSD = Open(PJDF_DEVICE_ID_SD_ADAFRUIT, 0);
    if (!PJDF_IS_VALID_HANDLE(hSD)) while(1);

    PrintWithBuf(buf, PRINTBUFMAX, "Opening SD SPI driver: %s\n", SD_SPI_DEVICE_ID);
    // We talk to the SD controller over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the SD driver.
    hSPI = Open(SD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hSD, PJDF_CTRL_SD_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);

    // Send initialization data to the MP3 decoder and run a test
	PrintWithBuf(buf, BUFSIZE, "Starting MP3 device test\n");
    Mp3Init(hMp3);
    int count = 0;

    while (1)
    {
        PrintWithBuf(buf, BUFSIZE, "Begin streaming sound file  count=%d\n", ++count);

        Mp3StreamSDFile(hMp3, hSD, "dogen.mp3");
        Mp3StreamSDFile(hMp3, hSD, "Oblivion.mp3");

        PrintWithBuf(buf, BUFSIZE, "Done streaming sound file  count=%d\n", count);
        OSTimeDly(OS_TICKS_PER_SEC * 3);

        //STATE == stop: wait for play command
        //STATE == play:
    }
}