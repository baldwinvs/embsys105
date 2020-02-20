#include "tasks.h"

#include "bsp.h"
#include "mp3Util.h"
#include "print.h"
#include "SD.h"

#include "InputCommands.h"
#include "PlayerState.h"

//Globals
extern OS_FLAG_GRP *rxFlags;       // Event flags for synchronizing mailbox messages

extern STATE state;

extern void setMp3Handle(HANDLE);

typedef struct _track_node
{
    char trackName[64];
    char fileName[13];
    uint32_t commaIndex;
    _track_node* next;
    _track_node* prev;
} TRACK_NODE;

void getFileList();
/************************************************************************************

Runs MP3 demo code

************************************************************************************/
void StreamingTask(void* pData)
{
    PjdfErrCode pjdfErr;
    INT32U length;

    OSTimeDly(OS_TICKS_PER_SEC * 2); // Allow other task to initialize LCD before we use it.

	PrintFormattedString("StreamingTask: starting\n");

	PrintFormattedString("Opening MP3 driver: %s\n", PJDF_DEVICE_ID_MP3_VS1053);
    // Open handle to the MP3 decoder driver
    HANDLE hMp3 = Open(PJDF_DEVICE_ID_MP3_VS1053, 0);
    if (!PJDF_IS_VALID_HANDLE(hMp3)) while(1);

    setMp3Handle(hMp3);

	PrintFormattedString("Opening MP3 SPI driver: %s\n", MP3_SPI_DEVICE_ID);
    // We talk to the MP3 decoder over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the MP3 driver.
    HANDLE hSPI = Open(MP3_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hMp3, PJDF_CTRL_MP3_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);

    // Initialize SD card
    PrintFormattedString("Opening handle to SD driver: %s\n", PJDF_DEVICE_ID_SD_ADAFRUIT);
    HANDLE hSD = Open(PJDF_DEVICE_ID_SD_ADAFRUIT, 0);
    if (!PJDF_IS_VALID_HANDLE(hSD)) while(1);

    PrintFormattedString("Opening SD SPI driver: %s\n", SD_SPI_DEVICE_ID);
    // We talk to the SD controller over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the SD driver.
    hSPI = Open(SD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hSD, PJDF_CTRL_SD_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);

    // Send initialization data to the MP3 decoder and run a test
//	PrintFormattedString("Starting MP3 device test\n");

    Mp3Init(hMp3);
    SD.begin(hSD);

    getFileList();
    {
        INT8U err;
        OSFlagPost(rxFlags, 1, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("StreamingTask: posting to flag group with error code %d\n", (INT32U)err);
        }
    }

    while (1)
    {
        switch(state) {
        case PS_PLAY:
            Mp3StreamSDFile(hMp3, "track009.mp3");
            Mp3StreamSDFile(hMp3, "track008.mp3");
            Mp3StreamSDFile(hMp3, "track001.mp3");
            Mp3StreamSDFile(hMp3, "track002.mp3");
            break;
        }
        OSTimeDly(OS_TICKS_PER_SEC * 3);
    }
}

void getFileList()
{
    File file = SD.open("songs.txt", O_READ);
    if (!file)
    {
        PrintFormattedString("Error: could not open SD card file 'songs.txt'\n");
        return;
    }

    char line[128];
    TRACK_NODE node;

    while(file.available()) {
        node = {0, 0, 0};
        memset(line, 0, 128);
        line[0] = file.read();
        if(line[0] == '\n') return;

        INT32U index = 1;
        while(index < BUFSIZE && line[index] != '\n') {
            line[index] = file.read();
            if(line[index] == '\r') {
                line[++index] = file.read();
                continue;
            }
            if(line[index] == ',') {
                node.commaIndex = index;
            }
            ++index;
        }

        strncpy(node.trackName, line, node.commaIndex - 4);
        strncpy(node.fileName, &line[node.commaIndex + 1], 12);

        PrintFormattedString("\tTrack: %s\tFile: %s\n", node.trackName, node.fileName);
    }
}