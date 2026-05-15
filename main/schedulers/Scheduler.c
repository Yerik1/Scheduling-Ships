#include "Scheduler.h"
#include <stdio.h>

/**
 * @brief First-Come, First-Served scheduling algorithm.
 * Always selects the first ship in the queue (index 0) if it's not empty.
 * @param q Pointer to the ReadyQueue to schedule from.
 * @return The index of the selected ship, or -1 if the queue is empty.
 */
int scheduler_fcfs(ReadyQueue *q)
{
    if (queue_is_empty(q))
    {
        return -1;
    }
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
    {
        return -1;
    }

    int best = 0;
    for (int i = 1; i < q->count; i++)
    {
        if (q->tasks[i]->ship.burst_time < q->tasks[best]->ship.burst_time)
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
    {
        return -1;
    }

    int best = -1;
    for (int i = 0; i < q->count; i++)
    {
        // Consider only ships that still have remaining time
        if (q->tasks[i]->ship.remaining_time <= 0)
        {
            continue;
        }

        if (best == -1 || q->tasks[i]->ship.remaining_time < q->tasks[best]->ship.remaining_time)
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
    {
        return -1;
    }

    int best = 0;
    for (int i = 1; i < q->count; i++)
    {
        if (q->tasks[i]->ship.priority < q->tasks[best]->ship.priority)
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
    if (queue_is_empty(q) || rr_index == NULL)
    {
        return -1;
    }
    if (quantum <= 0)
    {
        quantum = DEFAULT_QUANTUM;
    }

    // Adjust rr_index if it goes out of bounds due to ship removals
    if (*rr_index < 0 || *rr_index >= q->count)
    {
        *rr_index = 0;
    }

    int current = *rr_index;
    ShipTask *task = q->tasks[current];

    if (task == NULL)
    {
        return -1;
    }

    // Calculate ticks used by the current ship in this turn before decrementing remainingTime
    // (burstTime - remainingTime + 1) = total ticks used so far
    int ticks_used = (task->ship.burst_time - task->ship.remaining_time + 1);

    // Rotate to the next ship if the current one is finished or has used up its quantum
    int will_finish = (task->ship.remaining_time <= 1);
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
    {
        return -1;
    }

    int best = 0;
    for (int i = 1; i < q->count; i++)
    {
        if (q->tasks[i]->ship.deadline < q->tasks[best]->ship.deadline)
        {
            best = i;
        }
    }
    return best;
}
