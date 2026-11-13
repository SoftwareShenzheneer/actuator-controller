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

/* Build the initial screen. */
void ui_init(void);

/* Show the connected or disconnected BT overlay. */
void ui_set_bt_connected(bool connected);

#ifdef __cplusplus
}
#endif
