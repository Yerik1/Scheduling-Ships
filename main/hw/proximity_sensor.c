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

static void IRAM_ATTR proximity_sensor_isr_handler(void *args)
{
    (void) args;
    sensorTriggered = true;
}

bool proximity_sensor_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << PROXIMITY_SENSOR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = PROXIMITY_SENSOR_ACTIVE_HIGH ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE,
        .pull_down_en = PROXIMITY_SENSOR_ACTIVE_HIGH ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = PROXIMITY_SENSOR_ACTIVE_HIGH ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE
    };

    esp_err_t err = gpio_config(&config);

    if (err != ESP_OK) {
        printf("ERROR: No se pudo configurar GPIO del sensor de proximidad\n");
        return false;
    }

    err = gpio_install_isr_service(0);

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        printf("ERROR: No se pudo instalar ISR service de GPIO\n");
        return false;
    }

    err = gpio_isr_handler_add(
        PROXIMITY_SENSOR_GPIO,
        proximity_sensor_isr_handler,
        NULL
    );

    if (err != ESP_OK) {
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