#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "LinkedList.h"
#include "InputCommands.h"
#include "PlayerControl.h"

//Globals
extern OS_FLAG_GRP *rxFlags;       // Event flags for synchronizing mailbox messages
extern OS_EVENT * touch2CmdHandler;
extern OS_EVENT * commandMsgQ;

extern TRACK* head;
extern TRACK* current;

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
    CONTROL command = PC_NONE;
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

        INT32U cpu_sr;

        OS_ENTER_CRITICAL();

        //todo: refactor this ugly thing
        switch(state) {
        case PC_STOP:
            switch(*msgReceived) {
            case IC_PLAY:
                state = PC_PLAY;
                break;
            case IC_SKIP:
                current = current->next;
                break;
            case IC_RESTART:
                current = current->prev;
                break;
            case IC_VOLUP:
                control = PC_VOLUP;
                break;
            case IC_VOLDOWN:
                control = PC_VOLDOWN;
                break;
            default:
                break;
            }
            break;
        case PC_PLAY:
            switch(*msgReceived) {
            case IC_STOP:
                state = PC_STOP;
                break;
            case IC_PLAY:
                state = PC_PAUSE;
                break;
            case IC_SKIP:
                control = PC_SKIP;
                break;
            case IC_RESTART:
                control = PC_RESTART;
                break;
            case IC_VOLUP:
                control = PC_VOLUP;
                break;
            case IC_VOLDOWN:
                control = PC_VOLDOWN;
                break;
            default:
                break;
            }
            break;
        case PC_PAUSE:
            switch(*msgReceived) {
            case IC_STOP:
                break;
            case IC_PLAY:
                state = PC_PLAY;
                break;
            case IC_SKIP:
                state = PC_PLAY;
                control = PC_SKIP;
                break;
            case IC_RESTART:
                state = PC_PLAY;
                control = PC_RESTART;
                break;
            case IC_VOLUP:
                control = PC_VOLUP;
                break;
            case IC_VOLDOWN:
                control = PC_VOLDOWN;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
        command = (CONTROL)(state | control);
        OSQPost(commandMsgQ, &command);

        OS_EXIT_CRITICAL();

        control = PC_NONE;
    }
}

void setMp3Handle(HANDLE hMp3)
{
    _hMp3 = hMp3;
}