//
// Created by user on 4/27/2026.
//

#ifndef CANAL_H
#define CANAL_H

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "../queues/canal_list.h"

typedef struct
{
    int length;
    Direction current_direction;
    CanalList ships_inside;
    bool leftGateDown;
    bool rightGateDown;
    bool isBlocked;

    SemaphoreHandle_t stateSemaphore;
    SemaphoreHandle_t positionSemaphores[CANAL_LEN];
} Canal;

void canal_init(Canal *canal, int length, Direction initial_direction);
bool canal_is_empty(Canal *canal);
bool canal_can_enter(Canal *canal, ShipTask *task);
bool canal_enter(Canal *canal, ShipTask *task);
bool canal_can_move_task(Canal *canal, ShipTask *task);
bool canal_move_task(Canal *canal, ShipTask *task);
ShipTask *canal_remove_task(Canal *canal, ShipTask *task);
void canal_block(Canal *canal);
void canal_unblock(Canal *canal);
bool canal_move_one_step(Canal *canal, ShipTask *task);
void canal_notify_ships(Canal *canal);
void canal_notify_ships_ordered(Canal *canal);
void canal_set_ship_position(Canal *canal, ShipTask *task, int position);
bool canal_enter_at(Canal *canal, ShipTask *task, int position);

#endif // CANAL_H
