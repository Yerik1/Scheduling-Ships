//
// Created by user on 4/27/2026.
//

#include "ready_queue.h"

/**
 * @brief Sets queue count to 0, effectively clearing it.
 * @param q Pointer to the ReadyQueue to initialize.
 * @return void.
 */
void queue_init(ReadyQueue *q){
    if (q == NULL){
        return;
    }

    q->count = 0;

    for (int i = 0; i < MAX_SHIPS; i++){
        q->tasks[i] = NULL;
    }
}

/**
 * @brief Adds a ship to the ready queue if there's space.
 * @param q Pointer to the ReadyQueue to add the ship to.
 * @param task Pointer to the task of the Ship struct to add to the queue.
 * @return 1 if the ship was added successfully, 0 if the queue is full.
 */
int queue_add(ReadyQueue *q, ShipTask *task){
    if (q == NULL || task == NULL){
        return 0;
    }
    if (q->count >= MAX_SHIPS){
        return 0;
    }

    q->tasks[q->count++] = task;
    return 1;
}

/**
 * @brief Removes a ship from the ready queue at the specified index.
 * @param q Pointer to the ReadyQueue to remove the ship from.
 * @param index The index of the ship to remove.
 * @return 1 if the ship was removed successfully, 0 if the index is out of bounds.
 */
int queue_remove(ReadyQueue *q, int index){
    if (q == NULL){
        return 0;
    }
    if (index < 0 || index >= q->count){
        return 0;
    }
    for (int i = index; i < q->count - 1; i++){
        q->tasks[i] = q->tasks[i + 1];
    }
    q->tasks[q->count - 1] = NULL;
    q->count--;

    return 1;
}

/**
 * @brief Checks if the ready queue is empty.
 * @param q Pointer to the ReadyQueue to check.
 * @return 1 if the queue is empty, 0 otherwise.
 */
int queue_is_empty(const ReadyQueue *q){
    if (q == NULL){
        return 1;
    }
    return q->count == 0;
}

/**
 * @brief Gets an element form the queue
 * @param q Pointer to the ReadyQueue to check
 * @param index The index of the ship to get
 * @return the ship from the position of the queue or NULL if the index is out of range or the queue doesnt exist
 */
ShipTask *queue_get(ReadyQueue *q, int index){
    if (q == NULL){
        return NULL;
    }

    if (index < 0 || index >= q->count){
        return NULL;
    }

    return q->tasks[index];
}
