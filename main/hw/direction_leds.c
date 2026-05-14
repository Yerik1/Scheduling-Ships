//
// Created by user on 5/5/2026.
//
#include "direction_leds.h"

#include <stdio.h>

#include "driver/gpio.h"

#ifndef DIRECTION_LEFT_GPIO
#define DIRECTION_LEFT_GPIO 11
#endif

#ifndef DIRECTION_RIGHT_GPIO
#define DIRECTION_RIGHT_GPIO 10
#endif

bool direction_leds_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << DIRECTION_LEFT_GPIO) | (1ULL << DIRECTION_RIGHT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&config);

    if (err != ESP_OK) {
        printf("ERROR: No se pudieron configurar LEDs de direccion\n");
        return false;
    }

    direction_leds_clear();

    return true;
}

void direction_leds_set(FLOWDECISION direction)
{
    switch (direction) {
        case FLOW_LEFT:
            gpio_set_level(DIRECTION_LEFT_GPIO, 1);
            gpio_set_level(DIRECTION_RIGHT_GPIO, 0);
            break;

        case FLOW_RIGHT:
            gpio_set_level(DIRECTION_LEFT_GPIO, 0);
            gpio_set_level(DIRECTION_RIGHT_GPIO, 1);
            break;

        case FLOW_NONE:
        default:
            gpio_set_level(DIRECTION_LEFT_GPIO, 0);
            gpio_set_level(DIRECTION_RIGHT_GPIO, 0);
            break;
    }
}

void direction_leds_clear(void)
{
    gpio_set_level(DIRECTION_LEFT_GPIO, 0);
    gpio_set_level(DIRECTION_RIGHT_GPIO, 0);
}