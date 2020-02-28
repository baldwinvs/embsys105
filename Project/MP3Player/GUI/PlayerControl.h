#ifndef PLAYERCONTROL_H_
#define PLAYERCONTROL_H_

typedef enum player_control {
    PC_NONE    = 0x0000,
    PC_STOP    = 0x0001,
    PC_PLAY    = 0x0002,
    PC_PAUSE   = 0x0004,
    PC_FF      = 0x0008,
    PC_RWD     = 0x0010,
    PC_SKIP    = 0x0020,
    PC_RESTART = 0x0040,
    PC_VOLUP   = 0x0080,
    PC_VOLDOWN = 0x0100,
} CONTROL;

const uint32_t stateMask = 0x0000001F;
const uint32_t controlMask = 0x000001E0;

#endif /* PLAYERCONTROL_H_ */