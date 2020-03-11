#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "LinkedList.h"
#include "InputCommands.h"
#include "PlayerControl.h"

//Globals
extern OS_FLAG_GRP *initFlags;
extern OS_EVENT * touch2CmdHandler;
extern OS_EVENT * cmdHandler2Stream;
extern OS_EVENT * cmdHandler2LcdHandler;
extern CONTROL stateAndControl;
extern const uint32_t commandHandlerEventBit = 0x4;

extern TRACK* head;
extern TRACK* current;

void CommandHandlerTask(void* pData)
{
    // Nothing to initialize, post the CommandHandlerEvent bit to the flag group.
    {
        uint8_t err;
        OSFlagPost(initFlags, commandHandlerEventBit, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("CommandHandlerTask: posting to flag group with error code %d\n", (uint32_t)err);
            while(1);
        }
    }

    INPUT_COMMAND* msgReceived = NULL;
    uint8_t err;
    CONTROL state = PC_STOP;
    CONTROL control = PC_NONE;

    while(OS_TRUE) {
        // Wait forever for the input command to come from TouchPollingTask.
        msgReceived = (INPUT_COMMAND*)OSMboxPend(touch2CmdHandler, 0, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("CommandHandlerTask: pending on mboxA with error code %d\n", (uint32_t)err);
            while(1);
        }

        switch(*msgReceived) {
        case IC_STOP:
            switch(state) {
            case PC_STOP:
            case PC_PLAY:
            case PC_PAUSE:
                state = PC_STOP;    // always go to stop state
                break;
            default:
                break;
            }
            break;
        case IC_PLAY:
            switch(state) {
            case PC_PLAY:
                state = PC_PAUSE;   // play -> pause
                break;
            case PC_STOP:
            case PC_PAUSE:
            case PC_FF:
            case PC_RWD:
                state = PC_PLAY;    // all others go to play state
                break;
            default:
                break;
            }
            break;
        case IC_SKIP:
            switch(state) {
            case PC_STOP:
                control = PC_SKIP;  // no state change
                break;
            case PC_PLAY:
                control = PC_SKIP;  // no state change
                break;
            case PC_PAUSE:
                state = PC_PLAY;    // pause -> play
                control = PC_SKIP;
                break;
            default:
                break;
            }
            break;
        case IC_RESTART:
            switch(state) {
            case PC_STOP:
                control = PC_RESTART;   // no state change
                break;
            case PC_PLAY:
                control = PC_RESTART;   // no state change
                break;
            case PC_PAUSE:
                state = PC_PLAY;        // pause -> play
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
                state = PC_FF;  // play/pause -> fast forward
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
                state = PC_RWD; // play/pause -> rewind
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
                control = PC_VOLUP; // no state change
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
                control = PC_VOLDOWN;   // no state change
                break;
            default:
                break;
            }
            break;
        default:
            continue;
        }

        // Package the state and control up for mailbox delivery.
        stateAndControl = (CONTROL)(state | control);

        // Send mail to StreamingTask.
        uint8_t err = OSMboxPostOpt(cmdHandler2Stream, &stateAndControl, OS_POST_OPT_NONE);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("CommandHandlerTask: failed to post cmdHandler2Stream with error %d\n", (uint32_t)err);
        }

        // Send mail to LcdHandlerTask.
        err = OSMboxPostOpt(cmdHandler2LcdHandler, &stateAndControl, OS_POST_OPT_NONE);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("CommandHandlerTask: failed to post cmdHandler2Stream with error %d\n", (uint32_t)err);
        }

        // Reset the control because it is not persistent.
        control = PC_NONE;
    }
}