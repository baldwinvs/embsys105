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
extern CONTROL stateAndControl;

extern TRACK* head;
extern TRACK* current;

void CommandHandlerTask(void* pData)
{
    {
        INT8U err;
        OSFlagPost(rxFlags, 4, OS_FLAG_SET, &err);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("CommandHandlerTask: posting to flag group with error code %d\n", (INT32U)err);
            while(1);
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
            while(1);
        }

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
            case PC_FF:
            case PC_RWD:
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

        stateAndControl = (CONTROL)(state | control);
        uint8_t err = OSMboxPostOpt(cmdHandler2Stream, &stateAndControl, OS_POST_OPT_NONE);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("CommandHandlerTask: failed to post cmdHandler2Stream with error %d\n", (INT32U)err);
        }

        err = OSMboxPostOpt(cmdHandler2LcdHandler, &stateAndControl, OS_POST_OPT_NONE);
        if(OS_ERR_NONE != err) {
            PrintFormattedString("CommandHandlerTask: failed to post cmdHandler2Stream with error %d\n", (INT32U)err);
        }

        control = PC_NONE;
    }
}