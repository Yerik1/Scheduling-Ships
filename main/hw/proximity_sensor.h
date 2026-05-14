//
// Created by user on 5/5/2026.
//

#ifndef PROXIMITY_SENSOR_H
#define PROXIMITY_SENSOR_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

bool proximity_sensor_init(void);

bool proximity_sensor_was_triggered(void);
bool proximity_sensor_is_active(void);
void proximity_sensor_clear_trigger(void);

void proximity_sensor_set_notify_task(TaskHandle_t taskHandle);

#endif // PROXIMITY_SENSOR_H
