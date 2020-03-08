#include "tasks.h"

#include "bsp.h"
#include "mp3Util.h"
#include "print.h"
#include "SD.h"

#include "LinkedList.h"
#include "InputCommands.h"
#include "PlayerControl.h"

#include <stdlib.h>

//Globals
extern OS_FLAG_GRP *rxFlags;       // Event flags for synchronizing mailbox messages
extern OS_EVENT * cmdHandler2Stream;
extern OS_EVENT * stream2LcdHandler;
extern OS_EVENT * progressMessage;
extern char songTitle[SONGLEN];
extern float progressValue;

static uint32_t trackListSize = 0;
TRACK* head = NULL;
TRACK* current = NULL;

CONTROL state = PC_STOP;
CONTROL control = PC_NONE;
INT8U volume[4];

void initFileList();
void checkCommandMailbox();
void volumeControl(HANDLE, const BOOLEAN);
/************************************************************************************

Runs MP3 demo code

************************************************************************************/
void StreamingTask(void* pData)
{
    PjdfErrCode pjdfErr;
    INT32U length;

    OSTimeDly(OS_TICKS_PER_SEC * 2); // Allow other task to initialize LCD before we use it.

	PrintFormattedString("Opening MP3 driver: %s\n", PJDF_DEVICE_ID_MP3_VS1053);
    // Open handle to the MP3 decoder driver
    HANDLE hMp3 = Open(PJDF_DEVICE_ID_MP3_VS1053, 0);
    if (!PJDF_IS_VALID_HANDLE(hMp3)) {
        PrintFormattedString("Failure opening MP3 driver: %s\n", PJDF_DEVICE_ID_MP3_VS1053);
        while(1);
    }

    // We talk to the MP3 decoder over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the MP3 driver.
    HANDLE hSPI = Open(MP3_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) {
        PrintFormattedString("Failure opening MP3 SPI driver: %s\n", MP3_SPI_DEVICE_ID);
        while(1);
    }

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hMp3, PJDF_CTRL_MP3_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) {
        PrintFormattedString("Failure setting MP3 SPI handle\n");
        while(1);
    }

    // Initialize SD card
    HANDLE hSD = Open(PJDF_DEVICE_ID_SD_ADAFRUIT, 0);
    if (!PJDF_IS_VALID_HANDLE(hSD)) {
        PrintFormattedString("Failure opening SD driver: %s\n", PJDF_DEVICE_ID_SD_ADAFRUIT);
        while(1);
    }

    // We talk to the SD controller over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to
    // the SD driver.
    hSPI = Open(SD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) {
        PrintFormattedString("Failure opening SD SPI driver: %s\n", SD_SPI_DEVICE_ID);
        while(1);
    }

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hSD, PJDF_CTRL_SD_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) {
        PrintFormattedString("Failure setting SD SPI handle\n");
        while(1);
    }

    // initialize the volume
    memcpy(volume, BspMp3SetVol4040, 4);

    Mp3Init(hMp3);
    SD.begin(hSD);

    initFileList();
    {
        INT8U err;
        OSFlagPost(rxFlags, 1, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("StreamingTask: posting to flag group with error code %d\n", (INT32U)err);
        }
    }

    uint8_t err = OS_ERR_NONE;

    INT8U sendTrack = OS_TRUE;

    while (1)
    {
        checkCommandMailbox();

        switch(state) {
        case PC_PLAY:
            OSMboxPostOpt(progressMessage, &progressValue, OS_POST_OPT_NONE);
            Mp3StreamSDFile(hMp3, current->fileName);
            memcpy(songTitle, current->trackName, SONGLEN);
            sendTrack = OS_TRUE;
            break;
        case PC_STOP:
            switch(control) {
            case PC_SKIP:
                current = current->next;
                memcpy(songTitle, current->trackName, SONGLEN);
                sendTrack = OS_TRUE;
                break;
            case PC_RESTART:
                current = current->prev;
                memcpy(songTitle, current->trackName, SONGLEN);
                sendTrack = OS_TRUE;
                break;
            case PC_VOLUP:
                volumeControl(hMp3, OS_TRUE);
                break;
            case PC_VOLDOWN:
                volumeControl(hMp3, OS_FALSE);
                break;
            default:
                break;
            }
        }
        control = PC_NONE;

        if(OS_TRUE == sendTrack) {
            err = OSMboxPostOpt(stream2LcdHandler, songTitle, OS_POST_OPT_NONE);
            if(OS_ERR_NONE != err) {
                PrintFormattedString("StreamingTask: failed to post stream2LcdHandler with error %d\n", (INT32U)err);
            }
            sendTrack = OS_FALSE;
        }

        OSTimeDly(1);
    }
}

void initFileList()
{
    File file = SD.open("songs.txt", O_READ);
    if (!file)
    {
        PrintFormattedString("Error: could not open SD card file 'songs.txt'\n");
        while(1) {
            OSTimeDly(OS_TICKS_PER_SEC);
        }
    }

    const size_t ancillaryCharCount= 20;

    // TRACK LIST FORMAT IS:
    //  trackXXX.mXX, <track name>.mXX
    while(file.available()) {
        TRACK* track = (TRACK *) malloc(sizeof(TRACK));
        char line[128] = {0};
        memset(track->trackName, 0, SONGLEN);
        memset(track->fileName, 0, 13);

        line[0] = file.read();
        if(line[0] == '\n') return;

        INT32U index = 1;
        while(index < 128 && line[index] != '\n') {
            line[index] = file.read();
            if(line[index] == '\r') {
                line[++index] = file.read();
                continue;
            }

            ++index;
        }

        strncpy(track->fileName, line, 12); // trackxxx.mXX is 12 characters
        strncpy(track->trackName, &line[14], index + 1 - ancillaryCharCount); // subtracting 4 to remove ".mXX"

        if(NULL == head) {
            head = track;
            current = head;
        }
        else {
            current->next = track;
            TRACK* tmp = current;
            current = track;
            current->prev = tmp;
        }
        ++trackListSize;

        PrintFormattedString("\tFile: %s\tTrack: %s\n", track->fileName, track->trackName);
    }

    // Close the circular doubly linked list
    current->next = head;
    head->prev = current;

    //Reset current to point at the head of the DLL.
    current = head;

    //Now check that each file can be opened.
    //Remove nodes as necessary and free the space
    uint32_t i;
    for(i = 0; i < trackListSize; ++i) {
        file = SD.open(current->fileName, O_READ);

        if(!file) {
            PrintFormattedString("Error - File List Initialization: could not open SD card file '%s'\n"
                                 "Removing from song list.\n" , current->fileName);

            //Remove the current node
            TRACK* nextTrack = current->next;
            TRACK* prevTrack = current->prev;

            nextTrack->prev = prevTrack;
            prevTrack->next = nextTrack;

            current->next = NULL;
            current->prev = NULL;
            free(current);

            current = nextTrack;
        }
        else {
            file.close();
        }
    }

    // Set head to be the current file.
    current = head;
    // Set the song title
    memcpy(songTitle, current->trackName, SONGLEN);
}

void checkCommandMailbox()
{
    CONTROL * command = NULL;
    command = (CONTROL *)OSMboxAccept(cmdHandler2Stream);

    if(NULL == command) {
        return;
    }

    state = (CONTROL)(*command & stateMask);
    control = (CONTROL)(*command & controlMask);
}

void volumeControl(HANDLE hMp3, const BOOLEAN volume_up)
{
    INT32U length = BspMp3SetVolLen;
    INT8U vol_LR = volume[3];

    if(OS_TRUE == volume_up) {
        if(vol_LR == 0xFE)      vol_LR = 0x80;
        else if(vol_LR >= 0x10) vol_LR -= 0x10;
        else                    vol_LR = 0x00;
    }
    else {
        if(vol_LR < 0x80)   vol_LR += 0x10;
        else                vol_LR = 0xFE;
    }
    volume[2] = vol_LR;
    volume[3] = vol_LR;

    //Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);

    Write(hMp3, (void*)volume, &length);

    //Set MP3 driver to data mode (subsequent writes will be sent to decoder's data interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_DATA, 0, 0);
}
