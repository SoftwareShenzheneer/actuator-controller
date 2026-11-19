/**
 * @file      ui.c
 * @brief     LVGL UI: status-neutral background + BT connected/disconnected overlays.
 *
 * The three images in main/resources/ were converted from the PNG sources
 * into LVGL 8.4 descriptors:
 *   ui_background    536x240  LV_IMG_CF_TRUE_COLOR
 *   ui_connected     302x64   LV_IMG_CF_TRUE_COLOR_ALPHA
 *   ui_disconnected  325x63   LV_IMG_CF_TRUE_COLOR_ALPHA
 *
 * A button press cycles the display through a three-screen carousel:
 *   disconnected -> connected -> empty -> disconnected -> ...
 * The "empty" screen shows only the background, with no status overlay.
 */
#include <stdbool.h>
#include "esp_log.h"
#include "lvgl.h"
#include "ui.h"

static const char *TAG = "UI";

/* Overlay position over the background's status area. */
#define UI_OVERLAY_X 170
#define UI_OVERLAY_Y 88

extern const lv_img_dsc_t ui_background;
extern const lv_img_dsc_t ui_connected;
extern const lv_img_dsc_t ui_disconnected;

static lv_obj_t *overlay_connected;
static lv_obj_t *overlay_disconnected;
static ui_screen_t current_screen;

static void ui_show_screen(ui_screen_t screen)
{
    current_screen = screen;

    if (overlay_connected == NULL || overlay_disconnected == NULL) {
        return;
    }

    switch (screen) {
    case UI_SCREEN_CONNECTED:
        lv_obj_clear_flag(overlay_connected, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(overlay_disconnected, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SCREEN_DISCONNECTED:
        lv_obj_add_flag(overlay_connected, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(overlay_disconnected, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SCREEN_EMPTY:
    default:
        lv_obj_add_flag(overlay_connected, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(overlay_disconnected, LV_OBJ_FLAG_HIDDEN);
        break;
    }
}

void ui_next_screen(void)
{
    switch (current_screen) {
    case UI_SCREEN_DISCONNECTED:
        ui_show_screen(UI_SCREEN_CONNECTED);
        break;
    case UI_SCREEN_CONNECTED:
        ui_show_screen(UI_SCREEN_EMPTY);
        break;
    case UI_SCREEN_EMPTY:
    default:
        ui_show_screen(UI_SCREEN_DISCONNECTED);
        break;
    }
}

void ui_init(void)
{
    ESP_LOGI(TAG, "Display %dx%d | bg %dx%d | overlays %dx%d, %dx%d",
             (int)lv_disp_get_hor_res(NULL),
             (int)lv_disp_get_ver_res(NULL),
             (int)ui_background.header.w, (int)ui_background.header.h,
             (int)ui_connected.header.w, (int)ui_connected.header.h,
             (int)ui_disconnected.header.w, (int)ui_disconnected.header.h);

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(scr);

    /* Background must be created first so the overlays render above it. */
    lv_obj_t *bg = lv_img_create(scr);
    lv_img_set_src(bg, &ui_background);
    lv_obj_set_pos(bg, 0, 0);

    overlay_connected = lv_img_create(scr);
    lv_img_set_src(overlay_connected, &ui_connected);
    lv_obj_set_pos(overlay_connected, UI_OVERLAY_X, UI_OVERLAY_Y);

    overlay_disconnected = lv_img_create(scr);
    lv_img_set_src(overlay_disconnected, &ui_disconnected);
    lv_obj_set_pos(overlay_disconnected, UI_OVERLAY_X, UI_OVERLAY_Y);

    ui_show_screen(UI_SCREEN_DISCONNECTED);
}
