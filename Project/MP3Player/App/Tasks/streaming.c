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
extern OS_FLAG_GRP * initFlags;
extern OS_EVENT * cmdHandler2Stream;
extern OS_EVENT * stream2LcdHandler;
extern OS_EVENT * progressMessage;
extern char songTitle[SONGLEN];
extern float progressValue;
extern const uint32_t streamingEventBit = 0x1;

static uint32_t trackListSize = 0;

//! Pointer to the head track in the linked list.
TRACK* head = NULL;

//! Pointer to the current track in the linked list.
TRACK* current = NULL;

//! The current state for the streaming task.
CONTROL state = PC_STOP;
//! The control of the streaming task; used only for a single frame at a time.
CONTROL control = PC_NONE;

//! The volume array used for setting the volume in the VS1053b.
uint8_t volume[4];

/** @brief Initialize the file list from the SD card by parsing a song list that has mapped the file names to the actual song names.
 * 
 * @note Allocates/deallocates memory using malloc/free.
 * @note Could use uCOS memory partitions if the logic was done a bit differently
 * (to find the file count before creating the partition).
 */
void initFileList();

/** @brief Check for a valid state and control value received from the CommandHandlerTask.
 */
void checkCommandMailbox();

/** @brief Increase/decrease the volume level in the VS1053b device.
 * 
 * | Value | Volume Level (%) |
 * |:-----:|:----------------:|
 * | 0x00  | 100              |
 * | 0x10  | 90               |
 * | 0x20  | 80               |
 * | 0x30  | 70               |
 * | 0x40  | 60               |
 * | 0x50  | 50               |
 * | 0x60  | 40               |
 * | 0x70  | 30               |
 * | 0x80  | 20               |
 * | 0xFE  | 0                |
 * 
 * @note 10% volume was omitted because it was barely noticeable and the GUI
 * worked out nicely with only 10 steps for the volume.
 * 
 * @param hMp3 The MP3 device handle.
 * @param volume_up **OS_TRUE** for turning the volume up, **OS_FALSE** otherwise.
 */
void volumeControl(HANDLE hMp3, const BOOLEAN volume_up);

void StreamingTask(void* pData)
{
    uint8_t err;
    BOOLEAN sendTrack = OS_TRUE;
    PjdfErrCode pjdfErr;
    size_t length;

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

    // Initialize the volume array.
    memcpy(volume, BspMp3SetVol4040, 4);

    // Initialize the MP3 device and start the SD device.
    Mp3Init(hMp3);
    SD.begin(hSD);

    // Initialize the file list.
    initFileList();

    // Initialization complete, set the streaming event bit.;
    {
        OSFlagPost(initFlags, streamingEventBit, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("StreamingTask: posting to flag group with error code %d\n", (uint32_t)err);
        }
    }

    while (OS_TRUE)
    {
        checkCommandMailbox();

        // Handle the play/stop states.
        switch(state) {
        case PC_PLAY:
            // Send a progress value of 0 to reset the progress bar in the GUI.
            OSMboxPostOpt(progressMessage, &progressValue, OS_POST_OPT_NONE);
            // Stream the file pointed to by the current node of the linked list.
            Mp3StreamSDFile(hMp3, current->fileName);
            /* The song has changed at this point, copy the new song title and schedule for
             * it to be sent to the LcdHandlerTask.
             */
            memcpy(songTitle, current->trackName, SONGLEN);
            sendTrack = OS_TRUE;
            break;
        case PC_STOP:
            // Handle control inputs from the user.
            switch(control) {
            case PC_SKIP:
                // Go to next track, update song title, and schedule for it to be sent to LcdHandlerTask.
                current = current->next;
                memcpy(songTitle, current->trackName, SONGLEN);
                sendTrack = OS_TRUE;
                break;
            case PC_RESTART:
                // Go to previous track, update song title, and schedule for it to be sent to LcdHandlerTask.
                current = current->prev;
                memcpy(songTitle, current->trackName, SONGLEN);
                sendTrack = OS_TRUE;
                break;
            case PC_VOLUP:
                // Turn the volume up, if able to do so.
                volumeControl(hMp3, OS_TRUE);
                break;
            case PC_VOLDOWN:
                // Turn the volume down, if able to do so.
                volumeControl(hMp3, OS_FALSE);
                break;
            default:
                break;
            }
        }
        // Reset the control value until another is received.
        control = PC_NONE;

        if(OS_TRUE == sendTrack) {
            // Send the track name to the LcdHandlerTask for display.
            err = OSMboxPostOpt(stream2LcdHandler, songTitle, OS_POST_OPT_NONE);
            if(OS_ERR_NONE != err) {
                PrintFormattedString("StreamingTask: failed to post stream2LcdHandler with error %d\n", (INT32U)err);
            }
            sendTrack = OS_FALSE;
        }

        // Give up the processor for any touch inputs/GUI updates.
        OSTimeDly(1);
    }
}

void initFileList()
{
    // Open the song list for parsing.
    File file = SD.open("songs.txt", O_READ);
    if (!file)
    {
        PrintFormattedString("Error: could not open SD card file 'songs.txt'\n");
        while(1);
    }

    const size_t fileNameSize = 12; // character count of "trackXXX.XXX"
    const size_t trackNameStart = 14; // file name followed by ", " before the track name
    const size_t ancillaryCharCount= trackNameStart + 6; // starting location of the track name + ".mXX" + "\r\n"
    const size_t maxLineSize = 128;

    // TRACK LIST FORMAT IS:
    //  trackXXX.mXX, <track name>.mXX
    while(file.available()) {
        TRACK* track = (TRACK *) malloc(sizeof(TRACK));
        char line[maxLineSize] = {0};

        // Clear the track name and file name arrays.
        memset(track->trackName, 0, SONGLEN);
        memset(track->fileName, 0, 13); // todo: add FILE_NAME_LEN to project

        // Read the first byte and check for a newline character.
        line[0] = file.read();
        if(line[0] == '\n') return;

        size_t index = 1;
        while(index < maxLineSize && line[index] != '\n') {
            line[index] = file.read();
            if(line[index] == '\r') {
                line[++index] = file.read();
                continue;
            }

            ++index;
        }
		++index;

        // Copy data from the line array to the file name and track name.
        strncpy(track->fileName, line, fileNameSize);
        strncpy(track->trackName, &line[trackNameStart], index - ancillaryCharCount);

        // Start the linked list.
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

        // Output the song list to the user terminal.
        PrintFormattedString("\tFile: %s\tTrack: %s\n", track->fileName, track->trackName);
    }

    // Make the doubly linked list into a circular doubly linked list
    current->next = head;
    head->prev = current;

    //Reset current to point at the head of the linked list.
    current = head;

    //Now check that each file can be opened; remove nodes if not able to be open to free space.
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
    // Set the song title.
    memcpy(songTitle, current->trackName, SONGLEN);
}

void checkCommandMailbox()
{
    const CONTROL * command = (CONTROL *)OSMboxAccept(cmdHandler2Stream);

    if(NULL == command) {
        return;
    }

    // Retrieve the state and control values if the command is valid.
    state = (CONTROL)(*command & stateMask);
    control = (CONTROL)(*command & controlMask);
}

void volumeControl(HANDLE hMp3, const BOOLEAN volume_up)
{
    uint32_t length = BspMp3SetVolLen;
    // Get the current volume from the left channel.
    uint8_t vol_LR = volume[3];

    if(OS_TRUE == volume_up) {
        if(vol_LR == 0xFE)      vol_LR = 0x80;  // if at minimum volume, jump up to 0x80
        else if(vol_LR >= 0x10) vol_LR -= 0x10; // if not at maximum volume, decrement value by 0x10 (jump up 10% volume)
        else                    vol_LR = 0x00;  // if greater than 90%, go to maximum value (100%)
    }
    else {
        if(vol_LR < 0x80)   vol_LR += 0x10;     // if not at 0x80 then increment by 0x10 (drop down 10% volume)
        else                vol_LR = 0xFE;      // if less than 10%, go to minimum value (0%)
    }
    // Set the volume for the left and right channels.
    volume[2] = vol_LR;
    volume[3] = vol_LR;

    //Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);

    Write(hMp3, (void*)volume, &length);

    //Set MP3 driver to data mode (subsequent writes will be sent to decoder's data interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_DATA, 0, 0);
}
