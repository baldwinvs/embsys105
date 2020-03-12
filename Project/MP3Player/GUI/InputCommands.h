#ifndef INPUTCOMMANDS_H_
#define INPUTCOMMANDS_H_

//! Enumerated commands based on the button that was pressed in the GUI.
typedef enum input_command
{
    IC_NONE = -1,   //!< No command; should never be sent.
    IC_PLAY = 0,    //!< The play command; generated from pressing \ref play_btn.
    IC_RESTART,     //!< The restart command; generated from pressing \ref restart_btn.
    IC_SKIP,        //!< The skip command; generated from pressing \ref skip_btn.
    IC_VOLDOWN,     //!< The volume down command; generated from pressing \ref vol_down_btn.
    IC_VOLUP,       //!< The volume up command; generated from pressing \ref vol_up_btn.
    IC_STOP,        //!< The stop command; generated from holding \ref play_btn for 3 seconds.
    IC_FF,          //!< The fast forward command; generated from holding \ref skip_btn for 1 second.
    IC_RWD,         //!< The rewind command; generated from holding \ref restart_btn for 1 second.
} INPUT_COMMAND;

#endif /* INPUTCOMMANDS_H_ */