//
// Created by user on 4/27/2026.
//

#ifndef SHIP_TASK_H
#define SHIP_TASK_H

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ship.h"

#define SHIP_TASK_DEFAULT_STACK_SIZE 4096
#define SHIP_TASK_DEFAULT_RTOS_PRIORITY 5
#define SHIP_TASK_DEFAULT_STEPS 5

typedef struct ShipTask {
    struct Ship ship;
    TaskHandle_t handle;
    char taskName[16];
    int maxSteps;
} ShipTask;

BaseType_t createShipTask(struct Ship ship);

BaseType_t createShipTaskWithConfig(
    struct Ship ship,
    UBaseType_t rtosPriority,
    uint32_t stackSizeBytes,
    int maxSteps
);

#endif //SHIP_TASK_H
