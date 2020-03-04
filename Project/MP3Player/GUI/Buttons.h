#ifndef BUTTONS_H_
#define BUTTONS_H_

#include <stdint.h>

typedef enum shape {
    S_SQUARE = 1,
    S_TRIANGLE,
    S_CIRCLE,
} SHAPE;

typedef struct {
    uint16_t x;
    uint16_t y;
} TP;

typedef union property {
    uint32_t p;
    TP tp;
} P;

typedef struct s {
    SHAPE shape;    // The shape
    P p0;           // Square: x, Circle: x, Triangle: x0, y0
    P p1;           // Square: y, Circle: y, Triangle: x0, y0
    P p2;           // Square: w, Circle: r, Triangle: x0, y0
    uint32_t h;     // Height: only intended for use by Squares
} BTN;

const BTN play_circle = {S_CIRCLE, 120, 240, 75, 0};
const BTN restart_square = {S_SQUARE, 5, 240, 110, 75};
const BTN skip_square = {S_SQUARE, 125, 240, 110, 75};
const BTN vol_dwn = {S_SQUARE, 0, 0, 40, 40};
const BTN vol_up = {S_SQUARE, 200, 0, 40, 40};
const BTN vol_bar = {S_SQUARE, 38, 18, 162, 4};
const BTN play_triangle = {S_TRIANGLE, (173 | (240 << 16)), (93 | (194 << 16)), (93 | (286 << 16))}; //r = 13
const BTN pause_left_square = {S_SQUARE, 90, 194, 20, 92};
const BTN pause_right_square = {S_SQUARE, 130, 194, 20, 92};
static BTN vol_slider = {S_SQUARE, 136, 8, 4, 24};   // always starts at 60% volume, other positions handled in lcdHandler

const BTN restart_left_triangle = {S_TRIANGLE, (41 | (250 << 16)), (41 | (305 << 16)), (29 | (278 << 16))};
const BTN restart_right_triangle = {S_TRIANGLE, (28 | (250 << 16)), (28 | (305 << 16)), (16 | (278 << 16))};
const BTN restart_right_square = {S_SQUARE, 12, 250, 4, 55};

const BTN skip_left_triangle = {S_TRIANGLE, (199 | (250 << 16)), (199 | (305 << 16)), (211 | (278 << 16))};
const BTN skip_right_triangle = {S_TRIANGLE, (212 | (250 << 16)), (212 | (305 << 16)), (224 | (278 << 16))};
const BTN skip_right_square = {S_SQUARE, 225, 250, 4, 55};

const BTN progress_square = {S_SQUARE, 5, 50, 230, 16};
static BTN innerProgress_square = {S_SQUARE, 9, 54, 222, 8};

const BTN icon_array[] =
{
    vol_bar, play_triangle, pause_left_square, pause_right_square, restart_left_triangle, restart_right_triangle,
    restart_right_square, skip_left_triangle, skip_right_triangle, skip_right_square
};
const size_t icon_array_sz = sizeof(icon_array) / sizeof(BTN);

const BTN btn_array[] =
{
    play_circle, restart_square, skip_square, vol_dwn, vol_up
};
const size_t btn_array_sz = sizeof(btn_array) / sizeof(BTN);


#endif /* BUTTONS_H_ */