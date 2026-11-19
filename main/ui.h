/**
 * @file      ui.h
 * @brief     Application UI entry point.
 *
 * Call ui_init() once after LVGL is initialized and a display driver has been
 * registered. All functions expect the caller to hold the LVGL mutex
 * (example_lvgl_lock()) when called from a task other than the LVGL task.
 */
#pragma once

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Screens in the button-driven carousel. */
typedef enum {
    UI_SCREEN_DISCONNECTED,
    UI_SCREEN_CONNECTED,
    UI_SCREEN_EMPTY,
} ui_screen_t;

/* Build the initial screen. */
void ui_init(void);

/* Advance to the next screen in the carousel:
 * disconnected -> connected -> empty -> disconnected -> ... */
void ui_next_screen(void);

#ifdef __cplusplus
}
#endif
