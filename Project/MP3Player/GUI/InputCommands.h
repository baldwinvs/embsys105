#ifndef INPUTCOMMANDS_H_
#define INPUTCOMMANDS_H_

// mapped to the order in btn_array
typedef enum input_command
{
    IC_NONE = -1,
    IC_PLAY = 0,
    IC_RESTART,
    IC_SKIP,
    IC_VOLDOWN,
    IC_VOLUP,
    IC_STOP,
    IC_FF,
    IC_RWD,
} INPUT_COMMAND;

#endif /* INPUTCOMMANDS_H_ */