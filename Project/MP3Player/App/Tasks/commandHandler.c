#include "tasks.h"

#include "bsp.h"
#include "print.h"

#include "InputCommands.h"

#ifndef BUFSIZE
#define BUFSIZE 256
#endif

//Globals
extern OS_FLAG_GRP *rxFlags;       // Event flags for synchronizing mailbox messages
extern OS_EVENT * touch2CmdHandler;

void CommandHandlerTask(void* pData)
{
    char buf[BUFSIZE];
    PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: starting\n");

    INPUT_COMMAND* msgReceived = NULL;

    while(1) {
        uint8_t err = 0;
        OSFlagPend(rxFlags, 1, OS_FLAG_WAIT_CLR_ALL, 0, &err);
        if(OS_ERR_NONE != err) {
            PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: waiting for flag group with error code %d\n", (INT32U)err);
        }

        // TODO Mailbox 04: add code here to receive a message in msgReceived from mailbox mboxA
        msgReceived = (INPUT_COMMAND*)OSMboxPend(touch2CmdHandler, 0, &err);
        if(OS_ERR_NONE != err) {
            PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: pending on mboxA with error code %d\n", (INT32U)err);
        }

        switch(*msgReceived) {
        case INPUTCOMMAND_PLAY:
            PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: received \"PLAY\" command!\n");
            break;
        case INPUTCOMMAND_STOP:
            PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: received \"STOP\" command!\n");
            break;
        case INPUTCOMMAND_NEXT:
            PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: received \"NEXT TRACK\" command!\n");
            break;
        case INPUTCOMMAND_PREV:
            PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: received \"PREVIOUS TRACK\" command!\n");
            break;
        case INPUTCOMMAND_FF:
            PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: received \"FAST FORWARD\" command!\n");
            break;
        case INPUTCOMMAND_RWD:
            PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: received \"REWIND\" command!\n");
            break;
        case INPUTCOMMAND_VOLUP:
            PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: received \"VOLUME UP\" command!\n");
            break;
        case INPUTCOMMAND_VOLDOWN:
            PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: received \"VOLUME DOWN\" command!\n");
            break;
        default:
            PrintWithBuf(buf, BUFSIZE, "CommandHandlerTask: received INVALID command!\n");
            break;
        }
    }
}