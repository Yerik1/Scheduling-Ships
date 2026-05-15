#ifndef CANAL_LIST_H
#define CANAL_LIST_H

#include <stdbool.h>
#include "../tasks/ship_task.h"

#define CANAL_LEN 10

/**
 * @brief Structure representing the canal task list.
 */
typedef struct
{
    ShipTask *tasks[CANAL_LEN]; // Array of pointers to ship tasks
    int count;                  // Current number of tasks in the canal
} CanalList;

/* ========================================================================== */
/* LIFECYCLE AND STATE                                                        */
/* ========================================================================== */

/**
 * @brief Initializes the canal list as empty.
 */
void canal_list_init(CanalList *cl);

/**
 * @brief Checks if the canal list contains no tasks.
 */
bool canal_list_empty(CanalList *cl);

/**
 * @brief Checks if a specific index in the canal is available.
 */
bool is_pos_free(CanalList *cl, int index);

/* ========================================================================== */
/* ELEMENT OPERATIONS                                                         */
/* ========================================================================== */

/**
 * @brief Adds a new task to the end of the canal list.
 * @return int 0 on success, or an error code if the list is full.
 */
int canal_list_add(CanalList *cl, ShipTask *task);

/**
 * @brief Retrieves the task at the specified index without removing it.
 */
ShipTask *canal_list_get(CanalList *cl, int index);

/**
 * @brief Removes and returns the task at the specified index.
 */
ShipTask *canal_list_remove(CanalList *cl, int index);

/**
 * @brief Moves a task from a source index to a destination index.
 */
bool move_task(CanalList *cl, int oldIndex, int newIndex);

#endif // CANAL_LIST_H
