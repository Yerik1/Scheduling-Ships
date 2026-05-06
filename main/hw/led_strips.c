//
// Created by user on 5/5/2026.
//

#include "led_strips.h"

#include <stdio.h>

#include "led_strip.h"

#ifndef LEFT_QUEUE_LED_GPIO
#define LEFT_QUEUE_LED_GPIO 10
#endif

#ifndef RIGHT_QUEUE_LED_GPIO
#define RIGHT_QUEUE_LED_GPIO 12
#endif

#ifndef CANAL_LED_GPIO
#define CANAL_LED_GPIO 11
#endif

#ifndef LEFT_QUEUE_LED_COUNT
#define LEFT_QUEUE_LED_COUNT 4
#endif

#ifndef RIGHT_QUEUE_LED_COUNT
#define RIGHT_QUEUE_LED_COUNT 4
#endif

#ifndef CANAL_LED_COUNT
#define CANAL_LED_COUNT CANAL_LEN
#endif

#define LED_RMT_RESOLUTION_HZ (10 * 1000 * 1000)

static led_strip_handle_t leftQueueStrip = NULL;
static led_strip_handle_t rightQueueStrip = NULL;
static led_strip_handle_t canalStrip = NULL;

static bool create_led_strip(int gpio, int ledCount, led_strip_handle_t *outStrip);
static void render_queue(led_strip_handle_t strip, ReadyQueue *queue, int ledCount);
static void set_ship_color(led_strip_handle_t strip, int ledIndex, ShipType type);
static void set_empty_color(led_strip_handle_t strip, int ledIndex);

bool led_strips_init(void)
{
    bool ok = true;

    if (!create_led_strip(LEFT_QUEUE_LED_GPIO, LEFT_QUEUE_LED_COUNT, &leftQueueStrip)) {
        printf("ERROR: No se pudo inicializar tira LED de cola izquierda\n");
        ok = false;
    }

    if (!create_led_strip(RIGHT_QUEUE_LED_GPIO, RIGHT_QUEUE_LED_COUNT, &rightQueueStrip)) {
        printf("ERROR: No se pudo inicializar tira LED de cola derecha\n");
        ok = false;
    }

    if (!create_led_strip(CANAL_LED_GPIO, CANAL_LED_COUNT, &canalStrip)) {
        printf("ERROR: No se pudo inicializar tira LED del canal\n");
        ok = false;
    }

    led_strips_clear();

    return ok;
}

void led_strips_render_left_queue(ReadyQueue *leftQueue)
{
    render_queue(leftQueueStrip, leftQueue, LEFT_QUEUE_LED_COUNT);
}

void led_strips_render_right_queue(ReadyQueue *rightQueue)
{
    render_queue(rightQueueStrip, rightQueue, RIGHT_QUEUE_LED_COUNT);
}

void led_strips_render_canal(Canal *canal)
{
    if (canalStrip == NULL || canal == NULL) {
        return;
    }

    for (int i = 0; i < CANAL_LED_COUNT; i++) {
        set_empty_color(canalStrip, i);
    }

    int limit = canal->length;

    if (limit > CANAL_LED_COUNT) {
        limit = CANAL_LED_COUNT;
    }

    for (int i = 0; i < limit; i++) {
        ShipTask *task = canal->ships_inside.tasks[i];

        if (task != NULL) {
            set_ship_color(canalStrip, i, task->ship.type);
        }
    }

    led_strip_refresh(canalStrip);
}

void led_strips_clear(void)
{
    if (leftQueueStrip != NULL) {
        led_strip_clear(leftQueueStrip);
    }

    if (rightQueueStrip != NULL) {
        led_strip_clear(rightQueueStrip);
    }

    if (canalStrip != NULL) {
        led_strip_clear(canalStrip);
    }
}

static bool create_led_strip(int gpio, int ledCount, led_strip_handle_t *outStrip)
{
    if (outStrip == NULL || ledCount <= 0) {
        return false;
    }

    led_strip_config_t stripConfig = {
        .strip_gpio_num = gpio,
        .max_leds = ledCount,
        .led_model = LED_MODEL_SK6812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false
        }
    };

    led_strip_rmt_config_t rmtConfig = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false
        }
    };

    esp_err_t err = led_strip_new_rmt_device(&stripConfig, &rmtConfig, outStrip);

    if (err != ESP_OK) {
        printf("ERROR: led_strip_new_rmt_device fallo en GPIO %d\n", gpio);
        return false;
    }

    led_strip_clear(*outStrip);

    return true;
}

static void render_queue(led_strip_handle_t strip, ReadyQueue *queue, int ledCount)
{
    if (strip == NULL || queue == NULL) {
        return;
    }

    for (int i = 0; i < ledCount; i++) {
        set_empty_color(strip, i);
    }

    int limit = queue->count;

    if (limit > ledCount) {
        limit = ledCount;
    }

    for (int i = 0; i < limit; i++) {
        ShipTask *task = queue->tasks[i];

        if (task != NULL) {
            set_ship_color(strip, i, task->ship.type);
        }
    }

    led_strip_refresh(strip);
}

static void set_ship_color(led_strip_handle_t strip, int ledIndex, ShipType type)
{
    if (strip == NULL || ledIndex < 0) {
        return;
    }

    switch (type) {
        case NORMAL:
            led_strip_set_pixel(strip, ledIndex, 0, 32, 0);      // verde tenue
            break;

        case FISHING:
            led_strip_set_pixel(strip, ledIndex, 0, 0, 32);      // azul tenue
            break;

        case PATROL:
            led_strip_set_pixel(strip, ledIndex, 32, 0, 0);      // rojo tenue
            break;

        default:
            led_strip_set_pixel(strip, ledIndex, 16, 16, 16);    // blanco tenue
            break;
    }
}

static void set_empty_color(led_strip_handle_t strip, int ledIndex)
{
    if (strip == NULL || ledIndex < 0) {
        return;
    }

    led_strip_set_pixel(strip, ledIndex, 0, 0, 0);
}

bool led_strips_init_canal_only(void)
{
    if (!create_led_strip(CANAL_LED_GPIO, CANAL_LED_COUNT, &canalStrip)) {
        printf("ERROR: No se pudo inicializar tira LED del canal\n");
        return false;
    }

    if (canalStrip != NULL) {
        led_strip_clear(canalStrip);
    }

    return true;
}