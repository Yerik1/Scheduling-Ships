#include "led_strips.h"

#include <stdio.h>
#include <string.h>

#include "led_strip.h"
#include "esp_err.h"

// =========================
// Pines soldados
// =========================

#ifndef LEFT_QUEUE_LED_GPIO
#define LEFT_QUEUE_LED_GPIO 7
#endif

#ifndef CANAL_LED_GPIO
#define CANAL_LED_GPIO 6
#endif

#ifndef RIGHT_QUEUE_LED_GPIO
#define RIGHT_QUEUE_LED_GPIO 5
#endif

// =========================
// Cantidades físicas
// =========================

#ifndef LEFT_QUEUE_LED_COUNT
#define LEFT_QUEUE_LED_COUNT 4
#endif

#ifndef CANAL_LED_COUNT
#define CANAL_LED_COUNT 6
#endif

#ifndef RIGHT_QUEUE_LED_COUNT
#define RIGHT_QUEUE_LED_COUNT 4
#endif

#define LED_RMT_RESOLUTION_HZ (10 * 1000 * 1000)

// =========================
// Buffers de estado visual
// =========================

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} LedColor;

static LedColor leftQueueBuffer[LEFT_QUEUE_LED_COUNT];
static LedColor canalBuffer[CANAL_LED_COUNT];
static LedColor rightQueueBuffer[RIGHT_QUEUE_LED_COUNT];

// =========================
// Prototipos privados
// =========================

static bool create_temp_strip(int gpio, int ledCount, led_strip_handle_t *strip);
static void flush_buffer_to_gpio(int gpio, LedColor *buffer, int ledCount);

static void clear_buffer(LedColor *buffer, int ledCount);
static void render_queue_to_buffer(ReadyQueue *queue, LedColor *buffer, int ledCount, bool reverse, int hwVisibleQueueSlots);
static void render_canal_to_buffer(Canal *canal, LedColor *buffer, int ledCount);

static LedColor ship_color(ShipType type);
static LedColor empty_color(void);

// =========================
// API pública
// =========================

bool led_strips_init(void)
{
    clear_buffer(leftQueueBuffer, LEFT_QUEUE_LED_COUNT);
    clear_buffer(canalBuffer, CANAL_LED_COUNT);
    clear_buffer(rightQueueBuffer, RIGHT_QUEUE_LED_COUNT);

    // Prueba rápida: crea y libera cada tira una vez.
    led_strip_handle_t testStrip = NULL;

    if (!create_temp_strip(LEFT_QUEUE_LED_GPIO, LEFT_QUEUE_LED_COUNT, &testStrip)) {
        printf("ERROR: No se pudo probar tira izquierda en GPIO %d\n", LEFT_QUEUE_LED_GPIO);
        return false;
    }
    led_strip_clear(testStrip);
    led_strip_del(testStrip);

    testStrip = NULL;

    if (!create_temp_strip(CANAL_LED_GPIO, CANAL_LED_COUNT, &testStrip)) {
        printf("ERROR: No se pudo probar tira canal en GPIO %d\n", CANAL_LED_GPIO);
        return false;
    }
    led_strip_clear(testStrip);
    led_strip_del(testStrip);

    testStrip = NULL;

    if (!create_temp_strip(RIGHT_QUEUE_LED_GPIO, RIGHT_QUEUE_LED_COUNT, &testStrip)) {
        printf("ERROR: No se pudo probar tira derecha en GPIO %d\n", RIGHT_QUEUE_LED_GPIO);
        return false;
    }
    led_strip_clear(testStrip);
    led_strip_del(testStrip);

    led_strips_clear();

    printf("Tiras LED inicializadas en modo temporal RMT\n");
    printf("LEFT GPIO=%d LEDs=%d\n", LEFT_QUEUE_LED_GPIO, LEFT_QUEUE_LED_COUNT);
    printf("CANAL GPIO=%d LEDs=%d\n", CANAL_LED_GPIO, CANAL_LED_COUNT);
    printf("RIGHT GPIO=%d LEDs=%d\n", RIGHT_QUEUE_LED_GPIO, RIGHT_QUEUE_LED_COUNT);

    return true;
}

void led_strips_render_left_queue(ReadyQueue *leftQueue, int hwVisibleQueueSlots)
{
    clear_buffer(leftQueueBuffer, LEFT_QUEUE_LED_COUNT);

    /*
     * Si físicamente la cola izquierda se ve invertida,
     * cambia false por true.
     */
    render_queue_to_buffer(leftQueue, leftQueueBuffer, LEFT_QUEUE_LED_COUNT, false, hwVisibleQueueSlots);

    flush_buffer_to_gpio(
        LEFT_QUEUE_LED_GPIO,
        leftQueueBuffer,
        LEFT_QUEUE_LED_COUNT
    );
}

void led_strips_render_right_queue(ReadyQueue *rightQueue, int hwVisibleQueueSlots)
{
    clear_buffer(rightQueueBuffer, RIGHT_QUEUE_LED_COUNT);

    /*
     * Si físicamente la cola derecha se ve invertida,
     * cambia false por true.
     */
    render_queue_to_buffer(rightQueue, rightQueueBuffer, RIGHT_QUEUE_LED_COUNT, false, hwVisibleQueueSlots);

    flush_buffer_to_gpio(
        RIGHT_QUEUE_LED_GPIO,
        rightQueueBuffer,
        RIGHT_QUEUE_LED_COUNT
    );
}

void led_strips_render_canal(Canal *canal)
{
    clear_buffer(canalBuffer, CANAL_LED_COUNT);

    render_canal_to_buffer(canal, canalBuffer, CANAL_LED_COUNT);

    flush_buffer_to_gpio(
        CANAL_LED_GPIO,
        canalBuffer,
        CANAL_LED_COUNT
    );
}

void led_strips_clear(void)
{
    clear_buffer(leftQueueBuffer, LEFT_QUEUE_LED_COUNT);
    clear_buffer(canalBuffer, CANAL_LED_COUNT);
    clear_buffer(rightQueueBuffer, RIGHT_QUEUE_LED_COUNT);

    flush_buffer_to_gpio(
        LEFT_QUEUE_LED_GPIO,
        leftQueueBuffer,
        LEFT_QUEUE_LED_COUNT
    );

    flush_buffer_to_gpio(
        CANAL_LED_GPIO,
        canalBuffer,
        CANAL_LED_COUNT
    );

    flush_buffer_to_gpio(
        RIGHT_QUEUE_LED_GPIO,
        rightQueueBuffer,
        RIGHT_QUEUE_LED_COUNT
    );
}

// =========================
// Funciones privadas
// =========================

static bool create_temp_strip(int gpio, int ledCount, led_strip_handle_t *strip)
{
    if (strip == NULL || ledCount <= 0) {
        return false;
    }

    *strip = NULL;

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

        /*
         * En ESP32-C6 conviene mantener esto bajo.
         */
        .mem_block_symbols = 48,

        .flags = {
            .with_dma = false
        }
    };

    esp_err_t err = led_strip_new_rmt_device(
        &stripConfig,
        &rmtConfig,
        strip
    );

    if (err != ESP_OK) {
        printf(
            "ERROR: led_strip_new_rmt_device fallo en GPIO %d | err=%s\n",
            gpio,
            esp_err_to_name(err)
        );
        return false;
    }

    return true;
}

static void flush_buffer_to_gpio(int gpio, LedColor *buffer, int ledCount)
{
    if (buffer == NULL || ledCount <= 0) {
        return;
    }

    led_strip_handle_t strip = NULL;

    if (!create_temp_strip(gpio, ledCount, &strip)) {
        return;
    }

    for (int i = 0; i < ledCount; i++) {
        led_strip_set_pixel(
            strip,
            i,
            buffer[i].r,
            buffer[i].g,
            buffer[i].b
        );
    }

    led_strip_refresh(strip);

    /*
     * Liberamos el canal RMT para que la siguiente tira pueda usarlo.
     */
    led_strip_del(strip);
}

static void clear_buffer(LedColor *buffer, int ledCount)
{
    if (buffer == NULL || ledCount <= 0) {
        return;
    }

    LedColor off = empty_color();

    for (int i = 0; i < ledCount; i++) {
        buffer[i] = off;
    }
}

static void render_queue_to_buffer(
    ReadyQueue *queue,
    LedColor *buffer,
    int ledCount,
    bool reverse,
    int hwVisibleQueueSlots
) {
    if (queue == NULL || buffer == NULL || ledCount <= 0) {
        return;
    }

    int limit = queue->count;

    if (limit > hwVisibleQueueSlots)
    {
        limit = hwVisibleQueueSlots;
    }

    if (limit > ledCount) {
        limit = ledCount;
    }

    for (int i = 0; i < limit; i++) {
        ShipTask *task = queue->tasks[i];

        if (task == NULL) {
            continue;
        }

        int ledIndex = reverse ? (ledCount - 1 - i) : i;

        if (ledIndex < 0 || ledIndex >= ledCount) {
            continue;
        }

        buffer[ledIndex] = ship_color(task->ship.type);
    }
}

static void render_canal_to_buffer(Canal *canal, LedColor *buffer, int ledCount)
{
    if (canal == NULL || buffer == NULL || ledCount <= 0) {
        return;
    }

    if (canal->length <= 0) {
        return;
    }

    for (int pos = 0; pos < canal->length; pos++) {
        ShipTask *task = canal->ships_inside.tasks[pos];

        if (task == NULL) {
            continue;
        }

        /*
         * Mapeo proporcional:
         * Si canal->length = 6 y ledCount = 6:
         * pos 0 -> LED 0
         * pos 1 -> LED 1
         *
         * Si canal->length > 6:
         * varias posiciones lógicas se comprimen en 6 LEDs.
         */
        int ledIndex = (pos * ledCount) / canal->length;

        if (ledIndex < 0) {
            ledIndex = 0;
        }

        if (ledIndex >= ledCount) {
            ledIndex = ledCount - 1;
        }

        buffer[ledIndex] = ship_color(task->ship.type);
    }
}

static LedColor ship_color(ShipType type)
{
    LedColor color;

    switch (type) {
        case NORMAL:
            color.r = 0;
            color.g = 32;
            color.b = 0;
            break;

        case FISHING:
            color.r = 0;
            color.g = 0;
            color.b = 32;
            break;

        case PATROL:
            color.r = 32;
            color.g = 0;
            color.b = 0;
            break;

        default:
            color.r = 16;
            color.g = 16;
            color.b = 16;
            break;
    }

    return color;
}

static LedColor empty_color(void)
{
    LedColor color = {0, 0, 0};
    return color;
}