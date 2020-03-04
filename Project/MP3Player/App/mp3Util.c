/*
    mp3Util.c
    Some utility functions for controlling the MP3 decoder.

    Developed for University of Washington embedded systems programming certificate

    2016/2 Nick Strathy wrote/arranged it
*/

#include "bsp.h"
#include "print.h"
#include "SD.h"
#include "PlayerControl.h"
#include "LinkedList.h"

extern OS_EVENT * stream2LcdHandler;
extern OS_EVENT * progressMessage;
extern char songTitle[64];
extern float progressValue;

extern TRACK* head;
extern TRACK* current;
extern CONTROL state;
extern CONTROL control;
extern INT8U volume[4];

extern const uint8_t VOLUME_UP = 1;
extern const uint8_t VOLUME_DOWN = 0;

static File dataFile;

extern void checkCommandQueue();
extern void volumeControl(HANDLE, INT8U);

uint32_t checkRestartTrack(const uint32_t start)
{
    const uint32_t restartTime = OSTimeGet();
    const uint32_t diff = (restartTime < start ? (UINT32_MAX - start + restartTime) //inexplicable rollover occurred
                           : (restartTime - start));

    if(diff <= (2 * OS_TICKS_PER_SEC)) {
        return 1;
    }
    return 0;
}

static BOOLEAN writeNextSample(HANDLE hMp3)
{
    static INT8U mp3Buf[MP3_DECODER_BUF_SIZE] = {0};
    static INT32U bytesRead = 0;

    bytesRead = dataFile.read(mp3Buf, MP3_DECODER_BUF_SIZE);

    if(-1 == bytesRead) {
        PrintFormattedString("Error reading data file!\nPossible errors include:\n"
                             "\tread() called before a file has been opened,\n"
                             "\tcorrupt file system,\n"
                             "\tor an I/O error has occurred.\n");
        return OS_TRUE;
    }
    Write(hMp3, mp3Buf, &bytesRead);
    return OS_FALSE;
}

static void Mp3StreamInit(HANDLE hMp3)
{

    INT32U length;

    // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);

    // Reset the device
    length = BspMp3SoftResetLen;
    Write(hMp3, (void*)BspMp3SoftReset, &length);

    // Set volume
    length = BspMp3SetVolLen;
    Write(hMp3, (void*)volume, &length);

    // To allow streaming data, set the decoder mode to Play Mode
    length = BspMp3PlayModeLen;
    Write(hMp3, (void*)BspMp3PlayMode, &length);

    // Set MP3 driver to data mode (subsequent writes will be sent to decoder's data interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_DATA, 0, 0);
}

// Mp3StreamSDFile
// Streams the given file from the SD card to the given MP3 decoder.
// hMP3: an open handle to the MP3 decoder
// pFilename: The file on the SD card to stream.
void Mp3StreamSDFile(HANDLE hMp3, char *pFilename)
{
    INT32U length;

    Mp3StreamInit(hMp3);

    dataFile = SD.open(pFilename, O_READ);
    if (!dataFile)
    {
        PrintFormattedString("Error: could not open SD card file '%s'\n", pFilename);
        return;
    }

    INT32U startTime = OSTimeGet();
    BOOLEAN exit = OS_FALSE;
    INT8U iteration = 0;
    const INT8U progressSampleRate = 40;
    const INT8U fastForwardMultiplier = 20;
    const INT8U rewindMultiplier = 20;

    INT32S bytesAvailable = dataFile.available();
    const INT32U fileSize = dataFile.size();

    while (bytesAvailable && OS_FALSE == exit)
    {
        checkCommandQueue();

        switch(state) {
        case PC_STOP:
            exit = 1;
            current = head;
            memcpy(songTitle, current->trackName, 64);
            break;
        case PC_PLAY:
            exit = writeNextSample(hMp3);
            break;
        case PC_PAUSE:
            OSTimeDly(1);
            continue;
        case PC_FF:
            if(dataFile.size() - dataFile.position() > fastForwardMultiplier * MP3_DECODER_BUF_SIZE) {
                dataFile.seek(dataFile.position() + fastForwardMultiplier * MP3_DECODER_BUF_SIZE);
            }
            else {
                dataFile.seek(dataFile.size() - MP3_DECODER_BUF_SIZE);
            }

            exit = writeNextSample(hMp3);
            break;
        case PC_RWD:
            if(dataFile.position() > rewindMultiplier * MP3_DECODER_BUF_SIZE) {
                dataFile.seek(dataFile.position() - rewindMultiplier * MP3_DECODER_BUF_SIZE);
            }
            else {
                dataFile.seek(0);
            }

            exit = writeNextSample(hMp3);
            break;
        default:
            break;
        }

        if(PC_NONE  != control) {
            switch(control) {
            case PC_SKIP:
                current = current->next;
                memcpy(songTitle, current->trackName, 64);
                exit = OS_TRUE;
                break;
            case PC_RESTART:
                if(1 == checkRestartTrack(startTime)) {
                    current = current->prev;
                    memcpy(songTitle, current->trackName, 64);
                }
                exit = OS_TRUE;
                break;
            case PC_VOLUP:
                volumeControl(hMp3, VOLUME_UP);
                control = PC_NONE;
                break;
            case PC_VOLDOWN:
                volumeControl(hMp3, VOLUME_DOWN);
                control = PC_NONE;
                break;
            default:
                break;
            }
        }

        //update the bytes available
        bytesAvailable = dataFile.available();


        if(0 == iteration++ % progressSampleRate) {
            //send the progress
            progressValue = 100.0f * (float)dataFile.position() / fileSize;

            //don't report errors
            OSMboxPostOpt(progressMessage, &progressValue, OS_POST_OPT_NONE);
        }
    }

    //allow the track to change if played through
    if(PC_PLAY == state && PC_NONE == control) {
        current = current->next;
    }
    control = PC_NONE;
    progressValue = 0.0f;

    dataFile.close();

    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);
    length = BspMp3SoftResetLen;
    Write(hMp3, (void*)BspMp3SoftReset, &length);
}

// Mp3Stream
// Streams the given buffer of MP3 data to the given MP3 decoder
// hMp3: an open handle to the MP3 decoder
// pBuf: MP3 data to stream to the decoder
// bufLen: number of bytes of MP3 data to stream
void Mp3Stream(HANDLE hMp3, INT8U *pBuf, INT32U bufLen)
{

    INT8U *bufPos = pBuf;
    INT32U iBufPos = 0;
    INT32U length;
    INT32U chunkLen;
    BOOLEAN done = OS_FALSE;

    Mp3StreamInit(hMp3);

    chunkLen = MP3_DECODER_BUF_SIZE;

    while (!done)
    {
        // detect last chunk of pBuf
        if (bufLen - iBufPos < MP3_DECODER_BUF_SIZE)
        {
            chunkLen = bufLen - iBufPos;
            done = OS_TRUE;
        }

        Write(hMp3, bufPos, &chunkLen);

        bufPos += chunkLen;
        iBufPos += chunkLen;
    }

    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);
    length = BspMp3SoftResetLen;
    Write(hMp3, (void*)BspMp3SoftReset, &length);
}


// Mp3Init
// Send commands to the MP3 device to initialize it.
void Mp3Init(HANDLE hMp3)
{
    INT32U length;

    if (!PJDF_IS_VALID_HANDLE(hMp3)) while (1);

    // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);

    length = BspMp3SetClockFLen;
    Write(hMp3, (void*)BspMp3SetClockF, &length);

    length = BspMp3SetVolLen;
    Write(hMp3, (void*)BspMp3SetVol1010, &length);

    length = BspMp3SoftResetLen;
    Write(hMp3, (void*)BspMp3SoftReset, &length);
}

// Mp3GetRegister
// Gets a register value from the MP3 decoder.
// hMp3: an open handle to the MP3 decoder
// cmdInDataOut: on entry this buffer must contain the
//     command to get the desired register from the decoder. On exit
//     this buffer will be OVERWRITTEN by the data output by the decoder in
//     response to the command. Note: the buffer must not reside in readonly memory.
// bufLen: the number of bytes in cmdInDataOut.
// Returns: PJDF_ERR_NONE if no error otherwise an error code.
PjdfErrCode Mp3GetRegister(HANDLE hMp3, INT8U *cmdInDataOut, INT32U bufLen)
{
    PjdfErrCode retval;
    if (!PJDF_IS_VALID_HANDLE(hMp3)) while (1);

    // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);

    retval = Read(hMp3, cmdInDataOut, &bufLen);
    return retval;
}


// Mp3Test
// Runs sine wave sound test on the MP3 decoder.
// For VS1053, the sine wave test only works if run immediately after a hard
// reset of the chip.
void Mp3Test(HANDLE hMp3)
{
    INT32U length;

    if (!PJDF_IS_VALID_HANDLE(hMp3)) while (1);

    // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
    Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);

    // Make sure we can communicate with the device by sending a command to read a register value

    // First set volume to a known value
    length = BspMp3SetVolLen;
    Write(hMp3, (void*)BspMp3SetVol1010, &length);

    // Now get the volume setting on the device
    INT8U buf[10];
    memcpy(buf, BspMp3ReadVol, BspMp3ReadVolLen); // copy command from flash to a ram buffer
    Mp3GetRegister(hMp3, buf, BspMp3ReadVolLen);
    if (buf[2] != 0x10 || buf[3] != 0x10)
    {
        while(1); // failed to get data back from the device
    }

    for (int i = 0; i < 10; i++)
    {
        // set louder volume if i is odd
        if (i & 1)
        {
            length = BspMp3SetVolLen;
            Write(hMp3, (void*)BspMp3SetVol1010, &length);
        }
        else
        {
            length = BspMp3SetVolLen;
            Write(hMp3, (void*)BspMp3SetVol6060, &length);
        }

        // Put MP3 decoder in test mode
        length = BspMp3TestModeLen;
        Write(hMp3, (void*)BspMp3TestMode, &length);

        // Play a sine wave
        length = BspMp3SineWaveLen;
        Write(hMp3, (void*)BspMp3SineWave, &length);
        Write(hMp3, (void*)BspMp3SineWave, &length); // Sending once sometimes doesn't work!

        OSTimeDly(500);

        // Stop playing the sine wave
        length = BspMp3DeactLen;
        Write(hMp3, (void*)BspMp3Deact, &length);

        OSTimeDly(500);
    }
}
