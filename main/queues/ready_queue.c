#include "ready_queue.h"

/* ========================================================================== */
/* LIFECYCLE AND STATE                                                        */
/* ========================================================================== */

void queue_init(ReadyQueue *q)
{
    if (q == NULL)
    {
        return;
    }

    q->count = 0;
    for (int i = 0; i < READY_QUEUE_MAX_CAPACITY; i++)
    {
        q->tasks[i] = NULL;
    }
}

bool queue_is_empty(const ReadyQueue *q)
{
    if (q == NULL)
    {
        return true;
    }
    return q->count == 0;
}

/* ========================================================================== */
/* ELEMENT OPERATIONS                                                         */
/* ========================================================================== */

int queue_add(ReadyQueue *q, ShipTask *task)
{
    if (q == NULL || task == NULL || q->count >= READY_QUEUE_MAX_CAPACITY)
    {
        return 0;
    }

    // Insert at the end of the active elements and increment size
    q->tasks[q->count] = task;
    q->count++;

    return 1;
}

ShipTask *queue_get(ReadyQueue *q, int index)
{
    if (q == NULL || index < 0 || index >= q->count)
    {
        return NULL;
    }
    return q->tasks[index];
}

ShipTask *queue_remove(ReadyQueue *q, int index)
{
    if (q == NULL || index < 0 || index >= q->count)
    {
        return NULL;
    }

    // Store the task pointer to return it after shifting
    ShipTask *removed_task = q->tasks[index];

    // Array Shifting Logic:
    // Move all subsequent tasks one position to the left to keep
    // the array contiguous and prevent empty gaps in the middle of the queue.
    for (int i = index; i < q->count - 1; i++)
    {
        q->tasks[i] = q->tasks[i + 1];
    }

    // Clear the now unused last slot and decrement the total count
    q->tasks[q->count - 1] = NULL;
    q->count--;

    return removed_task;
}
