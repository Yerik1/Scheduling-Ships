//
// Created by user on 5/5/2026.
//

#ifndef PROXIMITY_SENSOR_H
#define PROXIMITY_SENSOR_H

#include <stdbool.h>

bool proximity_sensor_init(void);

bool proximity_sensor_was_triggered(void);

void proximity_sensor_clear_trigger(void);

#endif //PROXIMITY_SENSOR_H
