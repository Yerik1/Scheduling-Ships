#include "canal_list.h"

/* ========================================================================== */
/* LIFECYCLE AND STATE                                                        */
/* ========================================================================== */

void canal_list_init(CanalList *cl)
{
    if (cl == NULL)
    {
        return;
    }

    cl->count = 0;

    for (int i = 0; i < CANAL_LEN; i++)
    {
        cl->tasks[i] = NULL;
    }
}

bool canal_list_empty(CanalList *cl)
{
    if (cl == NULL)
    {
        return true;
    }
    return cl->count == 0;
}

bool is_pos_free(CanalList *cl, int index)
{
    if (cl == NULL || index < 0 || index >= CANAL_LEN)
    {
        return false;
    }
    if (cl->tasks[index] == NULL)
    {
        return true;
    }
    return false;
}

/* ========================================================================== */
/* ELEMENT OPERATIONS                                                         */
/* ========================================================================== */

int canal_list_add(CanalList *cl, ShipTask *task)
{
    if (cl == NULL || task == NULL || cl->count >= CANAL_LEN)
    {
        return 0;
    }

    // Directional Entry Logic:
    // Ships coming from the LEFT enter at index 0 (moving forward).
    // Ships coming from any other direction (RIGHT) enter at the last index.
    int entry_index = (task->ship.origin == LEFT) ? 0 : (CANAL_LEN - 1);

    // Prevent collision if the entry point is already occupied
    if (cl->tasks[entry_index] != NULL)
    {
        return 0;
    }

    // Place the ship in the canal and update its internal position state
    cl->tasks[entry_index] = task;
    task->ship.position = entry_index;
    cl->count++;

    return 1;
}

ShipTask *canal_list_get(CanalList *cl, int index)
{
    if (cl == NULL || index < 0 || index >= CANAL_LEN)
    {
        return NULL;
    }
    return cl->tasks[index];
}

ShipTask *canal_list_remove(CanalList *cl, int index)
{
    if (cl == NULL || index < 0 || index >= CANAL_LEN)
    {
        return NULL;
    }

    ShipTask *removedTask = cl->tasks[index];
    if (removedTask == NULL)
    {
        return NULL;
    }

    cl->tasks[index] = NULL;
    cl->count--;

    return removedTask;
}

bool move_task(CanalList *cl, int oldIndex, int newIndex)
{
    if (cl == NULL)
    {
        return false;
    }

    // Boundary checks for both source and destination
    if (oldIndex < 0 || oldIndex >= CANAL_LEN || newIndex < 0 || newIndex >= CANAL_LEN)
    {
        return false;
    }

    // Movement Logic:
    // Ensures there is a ship to move at the source, and the destination is free.
    if (cl->tasks[oldIndex] != NULL && cl->tasks[newIndex] == NULL)
    {
        cl->tasks[newIndex] = cl->tasks[oldIndex];
        cl->tasks[oldIndex] = NULL;
        cl->tasks[newIndex]->ship.position = newIndex; // Sync ship's internal position
        return true;
    }

    return false;
}
