#ifndef PLAYERCOMMAND_H_
#define PLAYERCOMMAND_H_

//! Enumerated commands bitmasked to state and control groups.
//! ***Awfully*** similar to InputCommands.
typedef enum player_command {
    PC_NONE    = 0x0000,    //!< No command.
    PC_STOP    = 0x0001,    //!< Stopped state.
    PC_PLAY    = 0x0002,    //!< Playing state.
    PC_PAUSE   = 0x0004,    //!< Paused state.
    PC_FF      = 0x0008,    //!< Fast Forwarding state.
    PC_RWD     = 0x0010,    //!< Rewinding state.
    PC_SKIP    = 0x0020,    //!< Skip control.
    PC_RESTART = 0x0040,    //!< Restart control.
    PC_VOLUP   = 0x0080,    //!< Volume up control.
    PC_VOLDOWN = 0x0100,    //!< Volume down control.
} COMMAND;

//! Bit mask to get a state value from a COMMAND.
const uint32_t stateMask = 0x0000001F;
//! Bit mask to get a control value from a COMMAND.
const uint32_t controlMask = 0x000001E0;

#endif /* PLAYERCOMMAND_H_ */