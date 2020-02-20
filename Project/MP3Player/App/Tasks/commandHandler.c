#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "InputCommands.h"
#include "PlayerState.h"

//Globals
extern OS_FLAG_GRP *rxFlags;       // Event flags for synchronizing mailbox messages
extern OS_EVENT * touch2CmdHandler;
extern OS_EVENT * semPause;

STATE state = PS_STOP;

HANDLE _hMp3;

void setMp3Handle(HANDLE);

void CommandHandlerTask(void* pData)
{
    PrintFormattedString("CommandHandlerTask: starting\n");

    {
        INT8U err;
        OSFlagPost(rxFlags, 4, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("CommandHandlerTask: posting to flag group with error code %d\n", (INT32U)err);
        }
    }

    INPUT_COMMAND* msgReceived = NULL;
    uint8_t err;

    while(1) {
        msgReceived = (INPUT_COMMAND*)OSMboxPend(touch2CmdHandler, 0, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("CommandHandlerTask: pending on mboxA with error code %d\n", (INT32U)err);
        }

        //START
        //STATE == STOP: wait for PLAY command
        //      INPUT CMD == PLAY:
        //              begin playing the initial track (track001.mp3)
        //STATE == PLAY: after the track is finished, play the next track
        //      INPUT CMD == STOP:
        //              stop playing the current track, reset to initial track (track001.mp3)
        //      INPUT CMD == PAUSE:
        //              make reading the sd file pend using a binary semaphore
        //      INPUT CMD == RESTART:
        //              restart the current track if greater than 2 seconds has elapsed
        //              go to the previous track if less than 2 seconds has elapsed and begin playing
        //                  *(use ring buffer of tracks)
        //      INPUT CMD == SKIP:
        //              go to the next track and begin playing
        //                  *(use ring buffer of tracks)
        //STATE == PAUSE: wait for PLAY, RESTART or SKIP command
        //      INPUT CMD == STOP:
        //              stop playing the current track, reset to initial track (track001.mp3)
        //      INPUT CMD == PLAY
        //              resume playing the current track
        //      INPUT CMD == RESTART:
        //              restart the current track if greater than 2 seconds has elapsed
        //              go to the previous track if less than 2 seconds has elapsed and begin playing
        //                  *(make ring buffer of tracks)
        //      INPUT CMD == SKIP:
        //              go to the next track and begin playing
        //                  *(use ring buffer of tracks)

        switch(state) {
        case PS_STOP:
            switch(*msgReceived) {
            case INPUTCOMMAND_PLAY:
                OSSemPend(semPause, 0, &err);
                state = PS_PLAY;
                OSSemPost(semPause);
                break;
            default:
                break;
            }
            break;
        case PS_PLAY:
            switch(*msgReceived) {
            case INPUTCOMMAND_STOP:
                OSSemPend(semPause, 0, &err);
                state = PS_STOP;
                OSSemPost(semPause);
                break;
            case INPUTCOMMAND_PLAY:
                OSSemPend(semPause, 0, &err);
                state = PS_PAUSE;
                OSSemPost(semPause);
                break;
            case INPUTCOMMAND_SKIP:
                break;
            case INPUTCOMMAND_RESTART:
                break;
            default:
                break;
            }
            break;
        case PS_PAUSE:
            switch(*msgReceived) {
            case INPUTCOMMAND_STOP:
                break;
            case INPUTCOMMAND_PLAY:
                OSSemPend(semPause, 0, &err);
                state = PS_PLAY;
                OSSemPost(semPause);
                break;
            case INPUTCOMMAND_SKIP:
                break;
            case INPUTCOMMAND_RESTART:
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }

//        INT32U length = 0;
//        switch(*msgReceived) {
//        case INPUTCOMMAND_VOLUP:
//            length = BspMp3SetVol1010Len;
//            Write(_hMp3, (void*)BspMp3SetVol1010, &length);
//            break;
//        case INPUTCOMMAND_VOLDOWN:
//            length = BspMp3SetVol1010Len;
//            Write(_hMp3, (void*)BspMp3SetVol6060, &length);
//            break;
//        case INPUTCOMMAND_PLAY:
//        case INPUTCOMMAND_STOP:
//        case INPUTCOMMAND_SKIP:
//        case INPUTCOMMAND_RESTART:
//        case INPUTCOMMAND_FF:
//        case INPUTCOMMAND_RWD:
//        default:
//            break;
//        }
    }
}

void setMp3Handle(HANDLE hMp3)
{
    _hMp3 = hMp3;
}