//
// Created by user on 4/27/2026.
//

#ifndef READY_QUEUE_H
#define READY_QUEUE_H
#include "../tasks/ship_task.h"

#define MAX_SHIPS 4
// Ready Queue struct
typedef struct
{
    ShipTask *tasks[MAX_SHIPS];
    int count;
} ReadyQueue;

// ReadyQueue functions
void queue_init(ReadyQueue *q);
int queue_add(ReadyQueue *q, ShipTask *task);
int queue_remove(ReadyQueue *q, int index);
int queue_is_empty(const ReadyQueue *q);
ShipTask *queue_get(ReadyQueue *q, int index);

#endif //READY_QUEUE_H
