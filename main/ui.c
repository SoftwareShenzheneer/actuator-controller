/**
 * @file      ui.c
 * @brief     LVGL UI: status-neutral background + BT connected/disconnected overlays.
 *
 * The three images in main/resources/ were converted from the PNG sources
 * into LVGL 8.4 descriptors:
 *   ui_background    536x240  LV_IMG_CF_TRUE_COLOR
 *   ui_connected     302x64   LV_IMG_CF_TRUE_COLOR_ALPHA
 *   ui_disconnected  325x63   LV_IMG_CF_TRUE_COLOR_ALPHA
 */
#include <stdbool.h>
#include "esp_log.h"
#include "lvgl.h"
#include "ui.h"

static const char *TAG = "UI";

/* Toggle the overlays every 2 s so the rendering can be checked without BT.
 * Set to 0 once ui_set_bt_connected() is called from the real BT event. */
#define UI_BT_DEMO_TOGGLE 1
#define UI_BT_DEMO_PERIOD_MS 2000

/* Overlay position over the background's status area. */
#define UI_OVERLAY_X 170
#define UI_OVERLAY_Y 88

extern const lv_img_dsc_t ui_background;
extern const lv_img_dsc_t ui_connected;
extern const lv_img_dsc_t ui_disconnected;

static lv_obj_t *overlay_connected;
static lv_obj_t *overlay_disconnected;
static bool bt_connected;

void ui_set_bt_connected(bool connected)
{
    bt_connected = connected;

    if (overlay_connected == NULL || overlay_disconnected == NULL) {
        return;
    }

    if (connected) {
        lv_obj_clear_flag(overlay_connected, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(overlay_disconnected, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(overlay_connected, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(overlay_disconnected, LV_OBJ_FLAG_HIDDEN);
    }
}

#if UI_BT_DEMO_TOGGLE
static void bt_demo_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_set_bt_connected(!bt_connected);
}
#endif

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

    ui_set_bt_connected(false);

#if UI_BT_DEMO_TOGGLE
    lv_timer_create(bt_demo_timer_cb, UI_BT_DEMO_PERIOD_MS, NULL);
#endif
}
