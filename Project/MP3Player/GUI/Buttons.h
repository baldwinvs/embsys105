#ifndef BUTTONS_H_
#define BUTTONS_H_

#include <stdint.h>

typedef struct square_button {
    char name[32];
    BOOLEAN drawButton;
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} SQUARE_BUTTON;

typedef struct circle_button {
    uint32_t x;
    uint32_t y;
    uint32_t r;
} CIRCLE_BUTTON;

typedef struct triangle {
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
} TRIANGLE;

//const SQUARE_BUTTON btn_array[] = {play, stop, next, prev, forward, rewind, volumeUp, volumeDown};
//const size_t btn_array_sz = sizeof(btn_array) / sizeof(SQUARE_BUTTON);

const CIRCLE_BUTTON play_circle = {120, 240, 75};
const SQUARE_BUTTON restart_square = {"", OS_TRUE, 10, 240, 105, 70};
const SQUARE_BUTTON skip_square = {"", OS_TRUE, 125, 240, 105, 70};
const SQUARE_BUTTON vol_dwn = {"-", OS_FALSE, 0, 0, 40, 40};
const SQUARE_BUTTON vol_up = {"+", OS_FALSE, 200, 0, 40, 40};
const SQUARE_BUTTON vol_bar = {"", OS_TRUE, 40, 18, 160, 4};
const TRIANGLE play_icon = {173, 240, 93, 194, 93, 286}; //r = 13
const SQUARE_BUTTON pause_left_icon = {"", OS_TRUE, 90, 194, 20, 92};
const SQUARE_BUTTON pause_right_icon = {"", OS_TRUE, 130, 194, 20, 92};

#endif /* BUTTONS_H_ */