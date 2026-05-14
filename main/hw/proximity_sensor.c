//
// Created by user on 5/5/2026.
//

#include "proximity_sensor.h"

#include <stdio.h>

#include "driver/gpio.h"
#include "esp_attr.h"

#ifndef PROXIMITY_SENSOR_GPIO
#define PROXIMITY_SENSOR_GPIO 3
#endif

#ifndef PROXIMITY_SENSOR_ACTIVE_HIGH
#define PROXIMITY_SENSOR_ACTIVE_HIGH 1
#endif

static volatile bool sensorTriggered = false;
static TaskHandle_t notifyTaskHandle = NULL;

static void IRAM_ATTR proximity_sensor_isr_handler(void *args)
{
    (void)args;
    sensorTriggered = true;

    if (notifyTaskHandle != NULL)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(notifyTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

bool proximity_sensor_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << PROXIMITY_SENSOR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = PROXIMITY_SENSOR_ACTIVE_HIGH ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE,
        .pull_down_en = PROXIMITY_SENSOR_ACTIVE_HIGH ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = PROXIMITY_SENSOR_ACTIVE_HIGH ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE};

    esp_err_t err = gpio_config(&config);

    if (err != ESP_OK)
    {
        printf("ERROR: No se pudo configurar GPIO del sensor de proximidad\n");
        return false;
    }

    err = gpio_install_isr_service(0);

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        printf("ERROR: No se pudo instalar ISR service de GPIO\n");
        return false;
    }

    err = gpio_isr_handler_add(
        PROXIMITY_SENSOR_GPIO,
        proximity_sensor_isr_handler,
        NULL);

    if (err != ESP_OK)
    {
        printf("ERROR: No se pudo agregar ISR handler del sensor\n");
        return false;
    }

    sensorTriggered = false;

    return true;
}

bool proximity_sensor_was_triggered(void)
{
    return sensorTriggered;
}

void proximity_sensor_clear_trigger(void)
{
    sensorTriggered = false;
}

bool proximity_sensor_is_active(void)
{
    int level = gpio_get_level(PROXIMITY_SENSOR_GPIO);

    if (PROXIMITY_SENSOR_ACTIVE_HIGH)
    {
        return level == 1;
    }

    return level == 0;
}

void proximity_sensor_set_notify_task(TaskHandle_t taskHandle)
{
    notifyTaskHandle = taskHandle;
}