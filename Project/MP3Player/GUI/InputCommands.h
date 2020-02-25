#ifndef INPUTCOMMANDS_H_
#define INPUTCOMMANDS_H_

typedef enum _input_command
{
    IC_NONE = -1,
    IC_PLAY = 0,
    IC_STOP,
    IC_SKIP,
    IC_RESTART,
    IC_FF,
    IC_RWD,
    IC_VOLUP,
    IC_VOLDOWN,
} INPUT_COMMAND;

#endif /* INPUTCOMMANDS_H_ */