#include "Scheduler.h"
#include <stdio.h>

/**
 * @brief Sets queue count to 0, effectively clearing it.
 * @param q Pointer to the ReadyQueue to initialize.
 * @return void.
 */
void queue_init(ReadyQueue *q)
{
    q->count = 0;
}

/**
 * @brief Adds a ship to the ready queue if there's space.
 * @param q Pointer to the ReadyQueue to add the ship to.
 * @param ship The Ship struct to add to the queue.
 * @return 1 if the ship was added successfully, 0 if the queue is full.
 */
int queue_add(ReadyQueue *q, struct Ship ship)
{
    if (q->count >= MAX_SHIPS)
        return 0;
    q->ships[q->count++] = ship;
    return 1;
}

/**
 * @brief Removes a ship from the ready queue at the specified index.
 * @param q Pointer to the ReadyQueue to remove the ship from.
 * @param index The index of the ship to remove.
 * @return 1 if the ship was removed successfully, 0 if the index is out of bounds.
 */
int queue_remove(ReadyQueue *q, int index)
{
    if (index < 0 || index >= q->count)
        return 0;
    for (int i = index; i < q->count - 1; i++)
    {
        q->ships[i] = q->ships[i + 1];
    }
    q->count--;
    return 1;
}

/**
 * @brief Checks if the ready queue is empty.
 * @param q Pointer to the ReadyQueue to check.
 * @return 1 if the queue is empty, 0 otherwise.
 */
int queue_is_empty(const ReadyQueue *q)
{
    return q->count == 0;
}

/**
 * @brief First-Come, First-Served scheduling algorithm.
 * Always selects the first ship in the queue (index 0) if it's not empty.
 * @param q Pointer to the ReadyQueue to schedule from.
 * @return The index of the selected ship, or -1 if the queue is empty.
 */
int scheduler_fcfs(ReadyQueue *q)
{
    if (queue_is_empty(q))
        return -1;
    return 0;
}

/**
 * @brief Shortest Job First scheduling algorithm.
 * Selects the ship with the smallest burstTime from the ready queue.
 * @param q Pointer to the ReadyQueue to schedule from.
 * @return The index of the selected ship, or -1 if the queue is empty.
 */
int scheduler_sjf(ReadyQueue *q)
{
    if (queue_is_empty(q))
        return -1;

    int best = 0;
    for (int i = 1; i < q->count; i++)
    {
        if (q->ships[i].burstTime < q->ships[best].burstTime)
        {
            best = i;
        }
    }
    return best;
}

/**
 * @brief Shortest Time Remaining Next scheduling algorithm (preemptive).
 * Selects the ship with the smallest remainingTime from the ready queue.
 * Decrements the remainingTime of the selected ship by 1 tick.
 * If the ship's remainingTime reaches 0, it is marked as finished.
 * @param q Pointer to the ReadyQueue to schedule from.
 * @return The index of the selected ship, or -1 if the queue is empty.
 */
int scheduler_strn(ReadyQueue *q)
{
    if (queue_is_empty(q))
        return -1;

    int best = 0;
    for (int i = 1; i < q->count; i++)
    {
        // Consider only ships that still have remaining time
        if (q->ships[i].remainingTime > 0 &&
            q->ships[i].remainingTime < q->ships[best].remainingTime)
        {
            best = i;
        }
    }

    return best;
}

/**
 * @brief Priority scheduling algorithm.
 * Selects the ship with the highest priority (lowest priority number) from the ready queue.
 * @param q Pointer to the ReadyQueue to schedule from.
 * @return The index of the selected ship, or -1 if the queue is empty.
 */
int scheduler_priority(ReadyQueue *q)
{
    if (queue_is_empty(q))
        return -1;

    int best = 0;
    for (int i = 1; i < q->count; i++)
    {
        if (q->ships[i].priority < q->ships[best].priority)
        {
            best = i;
        }
    }
    return best;
}

/**
 * @brief Round Robin scheduling algorithm.
 * Selects ships in a circular order, giving each ship a fixed time slice (quantum).
 * @param q Pointer to the ReadyQueue to schedule from.
 * @param quantum The number of ticks each ship gets per turn.
 * @param rr_index Pointer to the current index in the round-robin rotation (persistent between calls).
 * @return The index of the selected ship, or -1 if the queue is empty.
 */
int scheduler_rr(ReadyQueue *q, int quantum, int *rr_index)
{
    if (queue_is_empty(q))
        return -1;

    // Adjust rr_index if it goes out of bounds due to ship removals
    if (*rr_index >= q->count)
    {
        *rr_index = 0;
    }

    int current = *rr_index;
    struct Ship *ship = &q->ships[current];

    // Calculate ticks used by the current ship in this turn before decrementing remainingTime
    // (burstTime - remainingTime + 1) = total ticks used so far
    int ticks_used = (ship->burstTime - ship->remainingTime + 1);

    // Rotate to the next ship if the current one is finished or has used up its quantum
    int will_finish = (ship->remainingTime == 1);
    if (will_finish || (ticks_used % quantum == 0))
    {
        *rr_index = (*rr_index + 1) % q->count;
    }

    return current;
}

/**
 * @brief Earliest Deadline First scheduling algorithm (Real-Time).
 * Selects the ship with the earliest deadline from the ready queue.
 * @param q Pointer to the ReadyQueue to schedule from.
 * @return The index of the selected ship, or -1 if the queue is empty.
 */
int scheduler_edf(ReadyQueue *q)
{
    if (queue_is_empty(q))
        return -1;

    int best = 0;
    for (int i = 1; i < q->count; i++)
    {
        if (q->ships[i].deadline < q->ships[best].deadline)
        {
            best = i;
        }
    }
    return best;
}
