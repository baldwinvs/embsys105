#ifndef BUTTONS_H_
#define BUTTONS_H_

#include <stdint.h>

//! Enumeration defining the different shapes of buttons/icons used.
typedef enum shape {
    S_SQUARE = 1,
    S_TRIANGLE,
    S_CIRCLE,
} SHAPE;

//! Struct defining a vertex of a triangle.
typedef struct triangle_point{
    uint16_t x; //!< The x offset of the vertex.
    uint16_t y; //!< The y offset of the vertex.
} TP;

//! Union to hold a property of a shape (e.g. offset, radius, width, etc).
typedef union property {
    uint32_t p;
    TP tp;
} P;

//! Struct defining a button or icon.
typedef struct btn {
    SHAPE shape;    //!< The shape of the button/icon.
    P p0;           //!< Square: x, Circle: x, Triangle: x0, y0
    P p1;           //!< Square: y, Circle: y, Triangle: x0, y0
    P p2;           //!< Square: w, Circle: r, Triangle: x0, y0
    uint32_t h;     //!< Height: intended only for use by Square buttons/icons.
} BTN;

/** @defgroup icons GUI Icons
 * Icons and static elements to keep the user up to date on what is happening in the system.
 * 
 * @{
 */

/** @defgroup volume Volume Indicator Icons
 * Represents the current volume percentage in the system.
 * 
 * @{
 */

//! The volume bar.
const BTN vol_bar = {S_SQUARE, 38, 18, 162, 4};

/** The volume slider that adjusts when the volume is changed.
 * 
 * This icon is static to this file because LcdHandlerTask is responsible for positioning
 * after startup.
 * 
 * \note Volume always starts at 60%.
 */
static BTN vol_slider = {S_SQUARE, 132, 8, 4, 24};
/** @} */

//! The play button icon; only visible in STOPPED/PAUSED states.
const BTN play_icon = {S_TRIANGLE, (173 | (240 << 16)), (93 | (194 << 16)), (93 | (286 << 16))};

/** @defgroup pause Pause Button Icon
 * Two icons make up the pause button icon; only visible in PLAYING state.
 * 
 * @{
 */

//! The left portion of the pause button icon.
const BTN pause_left_icon = {S_SQUARE, 90, 194, 20, 92};
//! The right portion of the pause button icon.
const BTN pause_right_icon = {S_SQUARE, 130, 194, 20, 92};
/** @} */

/** @defgroup restart_rewind Restart/Rewind Icons
 * The individual icons that make up the restart/rewind icon.
 * 
 * @{
 */

//! The left triangle icon in the restart/rewind icon.
const BTN restart_left_icon = {S_TRIANGLE, (41 | (250 << 16)), (41 | (305 << 16)), (29 | (278 << 16))};
//! The right triangle icon in the restart/rewind icon.
const BTN restart_right_icon = {S_TRIANGLE, (28 | (250 << 16)), (28 | (305 << 16)), (16 | (278 << 16))};
//! The end bar icon in the restart/rewind icon.
const BTN restart_end_icon = {S_SQUARE, 12, 250, 4, 55};
/** @} */

/** @defgroup skip_fastforward Skip/Fast Forward Icons
 * The individual icons that make up the skip/fast forward icon.
 * 
 * @{
 */

//! The left triangle icon in the skip/fast forward icon.
const BTN skip_left_icon = {S_TRIANGLE, (199 | (250 << 16)), (199 | (305 << 16)), (211 | (278 << 16))};
//! The right triangle icon in the skip/fast forward icon.
const BTN skip_right_icon = {S_TRIANGLE, (212 | (250 << 16)), (212 | (305 << 16)), (224 | (278 << 16))};
//! The end bar icon in the skip/fast forward icon.
const BTN skip_end_icon = {S_SQUARE, 225, 250, 4, 55};
/** @} */

/** @defgroup progress Progress Bar Icons
 * The icons that make up the progress bar.
 * 
 * @{
 */

//! The outer progress bar icon (outline).
const BTN outer_progress_icon = {S_SQUARE, 5, 124, 230, 16};

//! The progress bar icon.
const BTN progress_icon = {S_SQUARE, 9, 128, 222, 8};
/** @} */

/** @} */

/** @defgroup pressable_buttons Pressable Buttons
 * Pressing any buttons in this group will generate a message from the TouchPollingTask
 * to the CommandHandlerTask.
 * 
 * @{
 */

/** The play button.
 * 
 * - Pressing will send the IC_PLAY command to CommandHandlerTask via the touch2CmdHandler
 * message mailbox.
 * 
 * - Hold this button for greater than 1 second to begin sending messages to LcdHandlerTask
 * via the touch2LcdHandler message mailbox.
 * 
 * - Continue holding for 2 more seconds to send the IC_STOP command to CommandHandlerTask
 * via the touch2CmdHandler message mailbox
 */
const BTN play_btn = {S_CIRCLE, 120, 240, 75, 0};

/** The restart button.
 * 
 * - Pressing will send the IC_RESTART command to CommandHandlerTask via the touch2CmdHandler
 * message mailbox.
 * 
 * - Hold this button for greater than 1 second to begin sending IC_RWD commands to
 * CommandHandlerTask via the touch2CmdHandler message mailbox.
 */
const BTN restart_btn = {S_SQUARE, 5, 240, 110, 75};

/** The skip button.
 * 
 * - Pressing will send the IC_SKIP command to CommandHandlerTask via the touch2CmdHandler
 * message mailbox.
 * 
 * - Hold this button for greater than 1 second to begin sending IC_FF commands to
 * CommandHandlerTask via the touch2CmdHandler message mailbox.
 */
const BTN skip_btn = {S_SQUARE, 125, 240, 110, 75};

/** The volume down button.
 * 
 * - Pressing will send the IC_VOLDOWN command to CommandHandlerTask via the touch2CmdHandler
 * message mailbox.
 * 
 * - No current functionality for holding the button in pressed state.
 */
const BTN vol_down_btn = {S_SQUARE, 0, 0, 40, 40};

/** The volume up button.
 * 
 * - Pressing will send the IC_VOLUP command to CommandHandlerTask via the touch2CmdHandler
 * message mailbox.
 * 
 * - No current functionality for holding the button in pressed state.
 */
const BTN vol_up_btn = {S_SQUARE, 200, 0, 40, 40};
/** @} */

//! The button array used for iterating through pressable buttons in TouchPollingTask.
//! The ordering must match the ordering of \ref input_command for the buttons in the array.
const BTN btn_array[] =
{
    play_btn, restart_btn, skip_btn, vol_down_btn, vol_up_btn
};

//! The size of btn_array in bytes.
const size_t btn_array_sz = sizeof(btn_array) / sizeof(BTN);


#endif /* BUTTONS_H_ */