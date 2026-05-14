#include "proximity_sensor.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Configuración de Pines
#define PROXIMITY_SENSOR_ECHO_GPIO 3
#define PROXIMITY_SENSOR_TRIG_GPIO 4
#define DISTANCE_THRESHOLD_CM 3

static volatile bool sensorTriggered = false;
static TaskHandle_t notifyTaskHandle = NULL;
static int64_t start_time = 0;

// Handler para medir la duración del pulso Echo
static void IRAM_ATTR proximity_sensor_isr_handler(void *args)
{
    int level = gpio_get_level(PROXIMITY_SENSOR_ECHO_GPIO);

    if (level == 1)
    {
        // Flanco de subida: inicia el conteo
        start_time = esp_timer_get_time();
    }
    else
    {
        // Flanco de bajada: calcula distancia
        int64_t time_diff = esp_timer_get_time() - start_time;
        float distance = (float)time_diff / 58.0;

        if (distance <= DISTANCE_THRESHOLD_CM && distance > 0)
        {
            sensorTriggered = true;
            if (notifyTaskHandle != NULL)
            {
                BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                vTaskNotifyGiveFromISR(notifyTaskHandle, &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
        }
    }
}

bool proximity_sensor_init(void)
{
    // Configurar pin de ECHO (Entrada con interrupción en cualquier cambio)
    gpio_config_t echo_conf = {
        .pin_bit_mask = (1ULL << PROXIMITY_SENSOR_ECHO_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE // Detectar subida y bajada
    };

    // Configurar pin de TRIGGER (Salida)
    gpio_config_t trig_conf = {
        .pin_bit_mask = (1ULL << PROXIMITY_SENSOR_TRIG_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    if (gpio_config(&echo_conf) != ESP_OK || gpio_config(&trig_conf) != ESP_OK)
    {
        return false;
    }

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PROXIMITY_SENSOR_ECHO_GPIO, proximity_sensor_isr_handler, NULL);

    sensorTriggered = false;
    return true;
}

// Esta función debe llamarse en un bucle para "lanzar" la medición
void proximity_sensor_trigger_ping(void)
{
    gpio_set_level(PROXIMITY_SENSOR_TRIG_GPIO, 0);
    esp_rom_delay_us(2);
    gpio_set_level(PROXIMITY_SENSOR_TRIG_GPIO, 1);
    esp_rom_delay_us(10);
    gpio_set_level(PROXIMITY_SENSOR_TRIG_GPIO, 0);
}

bool proximity_sensor_was_triggered(void)
{
    return sensorTriggered;
}

void proximity_sensor_clear_trigger(void)
{
    sensorTriggered = false;
}

void proximity_sensor_set_notify_task(TaskHandle_t taskHandle)
{
    notifyTaskHandle = taskHandle;
}
