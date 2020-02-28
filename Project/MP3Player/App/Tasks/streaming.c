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

static uint32_t trackListSize = 0;
TRACK* head = NULL;
TRACK* current = NULL;

CONTROL state = PC_STOP;
CONTROL control = PC_NONE;
INT8U volume[4];

const uint8_t VOLUME_UP = 1;
const uint8_t VOLUME_DOWN = 0;

void initFileList();
void checkCommandQueue();
void volumeControl(HANDLE, INT8U);
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

    while (1)
    {
        checkCommandQueue();

        switch(state) {
        case PC_PLAY:
            PrintFormattedString("Now playing: %s\n", current->trackName);
            Mp3StreamSDFile(hMp3, current->fileName);
            break;
        case PC_STOP:
            switch(control) {
            case PC_SKIP:
                current = current->next;
                break;
            case PC_RESTART:
                current = current->prev;
                break;
            case PC_VOLUP:
                volumeControl(hMp3, VOLUME_UP);
                break;
            case PC_VOLDOWN:
                volumeControl(hMp3, VOLUME_DOWN);
                break;
            default:
                break;
            }
        }
        control = PC_NONE;

        OSTimeDly(1);
    }
}

void initFileList()
{
    File file = SD.open("songs.txt", O_READ);
    if (!file)
    {
        PrintFormattedString("Error: could not open SD card file 'songs.txt'\n");
        return;
    }

    uint32_t commaIndex = 0;

    //TODO: reverse this so the format is
    //  trackXXX.mp3,<track name>
    while(file.available()) {
        //TODO: replace mallocs with memory partitions
        TRACK* track = (TRACK *) malloc(sizeof(TRACK));
        char line[128] = {0};
        memset(track->trackName, 0, 64);
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
            if(line[index] == ',') {
                commaIndex = index;
            }

            ++index;
        }

        strncpy(track->trackName, line, commaIndex - 4);     // subtracting 4 to remove ".mXX"
        strncpy(track->fileName, &line[commaIndex + 1], 12); // trackxxx.mXX is 12 characters

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
}

void checkCommandQueue()
{
    CONTROL * command = NULL;
    command = (CONTROL *)OSMboxAccept(cmdHandler2Stream);

    if(NULL == command) {
        return;
    }

    char stateStr[6] = {0};
    char controlStr[8] = {0};
    switch(*command & stateMask) {
    case PC_STOP:
        strcpy(stateStr, "STOP");
        state = (CONTROL)(*command & stateMask);
        break;
    case PC_PLAY:
        strcpy(stateStr, "PLAY");
        state = (CONTROL)(*command & stateMask);
        break;
    case PC_PAUSE:
        strcpy(stateStr, "PAUSE");
        state = (CONTROL)(*command & stateMask);
        break;
    default:
        break;
    }
    switch(*command & controlMask) {
    case PC_SKIP:
        strcpy(controlStr, "SKIP");
        control = (CONTROL)(*command & controlMask);
        break;
    case PC_RESTART:
        strcpy(controlStr, "RESTART");
        control = (CONTROL)(*command & controlMask);
        break;
    case PC_VOLUP:
        strcpy(controlStr, "VOL UP");
        control = (CONTROL)(*command & controlMask);
        break;
    case PC_VOLDOWN:
        strcpy(controlStr, "VOL DOWN");
        control = (CONTROL)(*command & controlMask);
        break;
    default:
        break;
    }

    PrintFormattedString("State changed to %s\n", stateStr);
    if(control != PC_NONE) {
        PrintFormattedString("Control changed to %s\n", controlStr);
    }
}

void volumeControl(HANDLE hMp3, INT8U vol_ctrl)
{
    INT32U length = BspMp3SetVolLen;
    INT8U vol_LR = volume[3];

    if(VOLUME_UP == vol_ctrl) {
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
