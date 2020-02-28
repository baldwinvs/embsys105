#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "LinkedList.h"
#include "InputCommands.h"
#include "PlayerControl.h"

//Globals
extern OS_FLAG_GRP *rxFlags;       // Event flags for synchronizing mailbox messages
extern OS_EVENT * touch2CmdHandler;
extern OS_EVENT * cmdHandler2Stream;
extern OS_EVENT * cmdHandler2LcdHandler;
extern CONTROL stateAndControl[1];

extern TRACK* head;
extern TRACK* current;

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
    CONTROL state = PC_STOP;
    CONTROL control = PC_NONE;

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
        switch(*msgReceived) {
        case IC_STOP:
            switch(state) {
            case PC_STOP:
            case PC_PLAY:
            case PC_PAUSE:
                state = PC_STOP;
                break;
            default:
                break;
            }
            break;
        case IC_PLAY:
            switch(state) {
            case PC_PLAY:
                state = PC_PAUSE;
                break;
            case PC_STOP:
            case PC_PAUSE:
                state = PC_PLAY;
                break;
            default:
                break;
            }
            break;
        case IC_SKIP:
            switch(state) {
            case PC_STOP:
                control = PC_SKIP;
                break;
            case PC_PLAY:
                control = PC_SKIP;
                break;
            case PC_PAUSE:
                state = PC_PLAY;
                control = PC_SKIP;
                break;
            default:
                break;
            }
            break;
        case IC_RESTART:
            switch(state) {
            case PC_STOP:
                control = PC_RESTART;
                break;
            case PC_PLAY:
                control = PC_RESTART;
                break;
            case PC_PAUSE:
                state = PC_PLAY;
                control = PC_RESTART;
                break;
            default:
                break;
            }
            break;
        case IC_FF:
            switch(state) {
            case PC_PLAY:
            case PC_PAUSE:
                state = PC_FF;
                break;
            case PC_STOP:
            default:
                break;
            }
            break;
        case IC_RWD:
            switch(state) {
            case PC_PLAY:
            case PC_PAUSE:
                state = PC_RWD;
                break;
            case PC_STOP:
            default:
                break;
            }
            break;
        case IC_VOLUP:
            switch(state) {
            case PC_STOP:
            case PC_PLAY:
            case PC_PAUSE:
                control = PC_VOLUP;
                break;
            default:
                break;
            }
            break;
        case IC_VOLDOWN:
            switch(state) {
            case PC_STOP:
            case PC_PLAY:
            case PC_PAUSE:
                control = PC_VOLDOWN;
                break;
            default:
                break;
            }
            break;
        default:
            continue;
        }

        stateAndControl[0] = (CONTROL)(state | control);
        uint8_t err = OSMboxPostOpt(cmdHandler2Stream, stateAndControl, OS_POST_OPT_NONE);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("CommandHandlerTask: failed to post cmdHandler2Stream with error %d\n", (INT32U)err);
        }

        err = OSMboxPostOpt(cmdHandler2LcdHandler, stateAndControl, OS_POST_OPT_NONE);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("CommandHandlerTask: failed to post cmdHandler2Stream with error %d\n", (INT32U)err);
        }

        control = PC_NONE;
    }
}