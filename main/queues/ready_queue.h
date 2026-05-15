#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include <stdbool.h>
#include "../tasks/ship_task.h"

#ifndef READY_QUEUE_MAX_CAPACITY
#define READY_QUEUE_MAX_CAPACITY 20
#endif

#ifndef READY_QUEUE_VISIBLE_SLOTS
#define READY_QUEUE_VISIBLE_SLOTS 4
#endif

/**
 * @brief Structure representing the priority or waiting queue for ready tasks.
 */
typedef struct
{
    ShipTask *tasks[READY_QUEUE_MAX_CAPACITY]; // Array of pointers to waiting ship tasks
    int count;                                 // Current number of tasks in the queue
} ReadyQueue;

/* ========================================================================== */
/* LIFECYCLE AND STATE                                                        */
/* ========================================================================== */

/**
 * @brief Initializes the ready queue as empty.
 */
void queue_init(ReadyQueue *q);

/**
 * @brief Checks if the ready queue contains no tasks.
 */
bool queue_is_empty(const ReadyQueue *q);

/* ========================================================================== */
/* ELEMENT OPERATIONS                                                         */
/* ========================================================================== */

/**
 * @brief Adds a new task to the end of the ready queue.
 * @return int 1 on success, or 0 if the queue is full or parameters are NULL.
 */
int queue_add(ReadyQueue *q, ShipTask *task);

/**
 * @brief Retrieves the task at the specified index without removing it.
 */
ShipTask *queue_get(ReadyQueue *q, int index);

/**
 * @brief Removes and returns the task at the specified index.
 * @return ShipTask* Pointer to the removed task, or NULL if index is invalid.
 */
ShipTask *queue_remove(ReadyQueue *q, int index);

#endif // READY_QUEUE_H
