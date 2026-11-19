/**
 * @file      main.c
 * @author    Lewis He (lewishe@outlook.com)
 * @license   MIT
 * @copyright Copyright (c) 2024  Shenzhen Xinyuan Electronic Technology Co., Ltd
 * @date      2024-01-07
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "amoled_driver.h"
#include "ui.h"
#include "tft_driver.h"
#include "product_pins.h"

static const char *TAG = "main";

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY 2

/* Button that cycles the UI carousel. BOARD_BUTTON1_PIN is the BOOT button
 * (GPIO0) on the T-Display S3 AMOLED. */
#define BUTTON_GPIO           BOARD_BUTTON1_PIN
#define BUTTON_DEBOUNCE_MS    50

static SemaphoreHandle_t lvgl_mux = NULL;
static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
lv_disp_drv_t disp_drv;      // contains callback functions
static QueueHandle_t button_evt_queue = NULL;

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
#if DISPLAY_FULLRESH
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );
    display_push_colors(area->x1, area->y1, w, h, (uint16_t *)color_map);
    lv_disp_flush_ready( drv );
#else
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    display_push_colors(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, (uint16_t *)color_map);
#endif
}

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

bool example_lvgl_lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void example_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

static void IRAM_ATTR button_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)(uintptr_t)arg;
    BaseType_t higher_priority_task_woken = pdFALSE;
    xQueueSendFromISR(button_evt_queue, &gpio_num, &higher_priority_task_woken);
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void button_task(void *arg)
{
    (void)arg;
    uint32_t gpio_num;

    while (true) {
        if (xQueueReceive(button_evt_queue, &gpio_num, portMAX_DELAY)) {
            /* Debounce: ignore the edge if the button is no longer held. */
            vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
            if (gpio_get_level((gpio_num_t)gpio_num) == 0) {
                if (example_lvgl_lock(-1)) {
                    ui_next_screen();
                    example_lvgl_unlock();
                }
            }
        }
    }
}

static void button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    button_evt_queue = xQueueCreate(1, sizeof(uint32_t));
    assert(button_evt_queue);

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler,
                                         (void *)(uintptr_t)BUTTON_GPIO));
    xTaskCreate(button_task, "button", 2048, NULL, 10, NULL);
}

/* Temporary UART ping/pong responder. The other board sends "ping"; this
 * replies "pong\n" on UART1 (GPIO10 RX / GPIO11 TX). Remove once verified. */
#define TEST_UART_NUM     UART_NUM_1
#define TEST_UART_TX_PIN  11
#define TEST_UART_RX_PIN  10
#define TEST_UART_BAUD    921600

static void uart_pingpong_task(void *arg)
{
    (void)arg;
    const uart_config_t uart_config = {
        .baud_rate = TEST_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(TEST_UART_NUM, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(TEST_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(TEST_UART_NUM, TEST_UART_TX_PIN, TEST_UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART1 ping/pong responder ready (GPIO10 RX / GPIO11 TX)");

    static const char ping[] = "ping";
    static const char pong[] = "pong\n";
    uint8_t byte;
    int match = 0;

    while (1) {
        if (uart_read_bytes(TEST_UART_NUM, &byte, 1, portMAX_DELAY) == 1) {
            ESP_LOGI(TAG, "Rx: '%c' (0x%02x)", byte, byte);
            if (byte == (uint8_t)ping[match]) {
                if (++match == (int)(sizeof(ping) - 1)) {
                    uart_write_bytes(TEST_UART_NUM, pong, sizeof(pong) - 1);
                    ESP_LOGI(TAG, "ping -> pong");
                    match = 0;
                }
            } else {
                match = (byte == (uint8_t)ping[0]) ? 1 : 0;
            }
        }
    }
}

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (true) {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (example_lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            example_lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

void app_main(void)
{
    display_init();
    lv_init();

    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
#if CONFIG_SPIRAM
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(DISPLAY_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(DISPLAY_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISPLAY_BUFFER_SIZE);
#else
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(AMOLED_HEIGHT * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = (lv_color_t *) heap_caps_malloc(AMOLED_HEIGHT * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, AMOLED_HEIGHT * 20);
#endif

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = AMOLED_HEIGHT;
    disp_drv.ver_res = AMOLED_WIDTH;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.full_refresh = DISPLAY_FULLRESH;
    lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick",
        .skip_unhandled_events = false
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);


    ESP_LOGI(TAG, "Display LVGL");
    if (example_lvgl_lock(-1)) {
        ui_init();
        example_lvgl_unlock();
    }
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

    button_init();

    xTaskCreate(uart_pingpong_task, "pingpong", 2048, NULL, 5, NULL);
}
