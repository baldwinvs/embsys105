#ifndef PLAYERCONTROL_H_
#define PLAYERCONTROL_H_

typedef enum player_control {
    PC_NONE    = 0,
    PC_STOP    = 1,
    PC_PLAY    = 2,
    PC_PAUSE   = 4,
    PC_SKIP    = 8,
    PC_RESTART = 16,
    PC_VOLUP   = 32,
    PC_VOLDOWN = 64,
} CONTROL;

const uint32_t stateMask = 0x00000007;
const uint32_t controlMask = 0x00000078;

#endif /* PLAYERCONTROL_H_ */