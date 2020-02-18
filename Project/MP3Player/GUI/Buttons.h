#ifndef BUTTONS_H_
#define BUTTONS_H_

#include <stdint.h>

typedef struct square_button {
    char primaryName[32];
    char secondaryName[32];
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} SQUARE_BUTTON;

const SQUARE_BUTTON play  = {"PLAY", "PAUSE", 10, 10, 100, 30};
const SQUARE_BUTTON stop  = {"STOP", "", 10, 50, 100, 30};

const SQUARE_BUTTON next = {"NEXT", "", 10, 90, 100, 30};
const SQUARE_BUTTON prev = {"PREV", "", 10, 130, 100, 30};

const SQUARE_BUTTON forward = {"FF", "", 10, 170, 100, 30};
const SQUARE_BUTTON rewind  = {"RWD", "", 10, 210, 100, 30};

//const SQUARE_BUTTON volumeUp   = {"UP", "", 10, 250, 50, 30};
//const SQUARE_BUTTON volumeDown = {"DOWN", "", 10, 290, 50, 30};

const SQUARE_BUTTON btn_array[] = {play, stop, next, prev, forward, rewind};

const size_t btn_array_sz = sizeof(btn_array) / sizeof(SQUARE_BUTTON);

#endif /* BUTTONS_H_ */