#include "canal.h"

#include <stdio.h>

static int get_entry_index(Canal *canal, ShipTask *task);
static int get_next_index(ShipTask *task);
static int find_task_position(Canal *canal, ShipTask *task);

static bool take_state_semaphore(Canal *canal);
static void give_state_semaphore(Canal *canal);

static bool take_position_semaphore(Canal *canal, int index);
static void give_position_semaphore(Canal *canal, int index);

static bool canal_add_task_at(Canal *canal, ShipTask *task, int index);

void canal_init(Canal *canal, int length, Direction initial_direction) {
    if (canal == NULL) {
        return;
    }

    if (length <= 0 || length > CANAL_LEN) {
        length = CANAL_LEN;
    }

    canal->length = length;
    canal->current_direction = initial_direction;

    canal->leftGateDown = false;
    canal->rightGateDown = false;
    canal->isBlocked = false;

    canal_list_init(&canal->ships_inside);

    canal->stateSemaphore = xSemaphoreCreateBinary();

    if (canal->stateSemaphore == NULL) {
        printf("ERROR: No se pudo crear stateSemaphore del canal\n");
        return;
    }

    xSemaphoreGive(canal->stateSemaphore);

    for (int i = 0; i < CANAL_LEN; i++) {
        canal->positionSemaphores[i] = xSemaphoreCreateBinary();

        if (canal->positionSemaphores[i] == NULL) {
            printf("ERROR: No se pudo crear semaforo para posicion %d\n", i);
        } else {
            xSemaphoreGive(canal->positionSemaphores[i]);
        }
    }
}

bool canal_is_empty(Canal *canal) {
    if (canal == NULL) {
        return true;
    }

    if (!take_state_semaphore(canal)) {
        return true;
    }

    bool result = canal_list_empty(&canal->ships_inside);

    give_state_semaphore(canal);

    return result;
}

bool canal_can_enter(Canal *canal, ShipTask *task) {
    if (canal == NULL || task == NULL) {
        return false;
    }

    int entryIndex = get_entry_index(canal, task);

    if (entryIndex < 0 || entryIndex >= canal->length) {
        return false;
    }

    bool result = false;

    if (!take_state_semaphore(canal)) {
        return false;
    }

    if (!take_position_semaphore(canal, entryIndex)) {
        give_state_semaphore(canal);
        return false;
    }

    if (canal->isBlocked) {
        result = false;
    } else if (task->ship.origin == LEFT && canal->leftGateDown) {
        result = false;
    } else if (task->ship.origin == RIGHT && canal->rightGateDown) {
        result = false;
    } else if (!canal_list_empty(&canal->ships_inside) &&
               canal->current_direction != task->ship.origin) {
        result = false;
    } else if (!is_pos_free(&canal->ships_inside, entryIndex)) {
        result = false;
    } else {
        result = true;
    }

    give_position_semaphore(canal, entryIndex);
    give_state_semaphore(canal);

    return result;
}

bool canal_enter(Canal *canal, ShipTask *task) {
    if (canal == NULL || task == NULL) {
        return false;
    }

    int entryIndex = get_entry_index(canal, task);

    if (entryIndex < 0 || entryIndex >= canal->length) {
        return false;
    }

    bool result = false;

    if (!take_state_semaphore(canal)) {
        return false;
    }

    if (!take_position_semaphore(canal, entryIndex)) {
        give_state_semaphore(canal);
        return false;
    }

    if (canal->isBlocked) {
        result = false;
    } else if (task->ship.origin == LEFT && canal->leftGateDown) {
        result = false;
    } else if (task->ship.origin == RIGHT && canal->rightGateDown) {
        result = false;
    } else if (!canal_list_empty(&canal->ships_inside) &&
               canal->current_direction != task->ship.origin) {
        result = false;
    } else if (!is_pos_free(&canal->ships_inside, entryIndex)) {
        result = false;
    } else {
        canal->current_direction = task->ship.origin;

        result = canal_add_task_at(canal, task, entryIndex);

        if (result) {
            task->ship.position = entryIndex;
            setState(&task->ship, RUNNING);

            printf(
                "Barco %d entro al canal en posicion %d\n",
                task->ship.id,
                entryIndex
            );
        }
    }

    give_position_semaphore(canal, entryIndex);
    give_state_semaphore(canal);

    return result;
}

bool canal_can_move_task(Canal *canal, ShipTask *task) {
    if (canal == NULL || task == NULL) {
        return false;
    }

    bool result = false;

    if (!take_state_semaphore(canal)) {
        return false;
    }

    if (canal->isBlocked) {
        give_state_semaphore(canal);
        return false;
    }

    int oldIndex = find_task_position(canal, task);

    if (oldIndex == -1) {
        give_state_semaphore(canal);
        return false;
    }

    int nextIndex = get_next_index(task);

    if (nextIndex < 0 || nextIndex >= canal->length) {
        give_state_semaphore(canal);
        return true;
    }

    if (!take_position_semaphore(canal, nextIndex)) {
        give_state_semaphore(canal);
        return false;
    }

    if (is_pos_free(&canal->ships_inside, nextIndex)) {
        result = true;
    }

    give_position_semaphore(canal, nextIndex);
    give_state_semaphore(canal);

    return result;
}

bool canal_move_task(Canal *canal, ShipTask *task) {
    if (canal == NULL || task == NULL) {
        return false;
    }

    bool result = false;

    if (!take_state_semaphore(canal)) {
        return false;
    }

    if (canal->isBlocked) {
        give_state_semaphore(canal);
        return false;
    }

    int oldIndex = find_task_position(canal, task);

    if (oldIndex == -1) {
        give_state_semaphore(canal);
        return false;
    }

    int nextIndex = get_next_index(task);

    if (nextIndex < 0 || nextIndex >= canal->length) {
        if (!take_position_semaphore(canal, oldIndex)) {
            give_state_semaphore(canal);
            return false;
        }

        ShipTask *removedTask = canal_list_remove(&canal->ships_inside, oldIndex);

        if (removedTask != NULL) {
            finish(&removedTask->ship);

            printf(
                "Barco %d salio del canal desde posicion %d\n",
                removedTask->ship.id,
                oldIndex
            );

            result = true;
        }

        give_position_semaphore(canal, oldIndex);
        give_state_semaphore(canal);

        return result;
    }

    int firstLock = oldIndex < nextIndex ? oldIndex : nextIndex;
    int secondLock = oldIndex < nextIndex ? nextIndex : oldIndex;

    if (!take_position_semaphore(canal, firstLock)) {
        give_state_semaphore(canal);
        return false;
    }

    if (!take_position_semaphore(canal, secondLock)) {
        give_position_semaphore(canal, firstLock);
        give_state_semaphore(canal);
        return false;
    }

    if (is_pos_free(&canal->ships_inside, nextIndex)) {
        result = move_task(&canal->ships_inside, oldIndex, nextIndex);

        if (result) {
            task->ship.position = nextIndex;
            decRemainingTime(&task->ship);

            printf(
                "Barco %d se movio de %d a %d\n",
                task->ship.id,
                oldIndex,
                nextIndex
            );
        }
    }

    give_position_semaphore(canal, secondLock);
    give_position_semaphore(canal, firstLock);
    give_state_semaphore(canal);

    return result;
}

ShipTask *canal_remove_task(Canal *canal, ShipTask *task) {
    if (canal == NULL || task == NULL) {
        return NULL;
    }

    if (!take_state_semaphore(canal)) {
        return NULL;
    }

    int index = find_task_position(canal, task);

    if (index == -1) {
        give_state_semaphore(canal);
        return NULL;
    }

    if (!take_position_semaphore(canal, index)) {
        give_state_semaphore(canal);
        return NULL;
    }

    ShipTask *removedTask = canal_list_remove(&canal->ships_inside, index);

    if (removedTask != NULL) {
        rstPosition(&removedTask->ship);
        setState(&removedTask->ship, READY);

        printf(
            "Barco %d fue removido del canal y vuelve a READY\n",
            removedTask->ship.id
        );
    }

    give_position_semaphore(canal, index);
    give_state_semaphore(canal);

    return removedTask;
}

void canal_block(Canal *canal) {
    if (canal == NULL) {
        return;
    }

    if (!take_state_semaphore(canal)) {
        return;
    }

    canal->isBlocked = true;
    canal->leftGateDown = true;
    canal->rightGateDown = true;

    give_state_semaphore(canal);
}

void canal_unblock(Canal *canal) {
    if (canal == NULL) {
        return;
    }

    if (!take_state_semaphore(canal)) {
        return;
    }

    canal->isBlocked = false;
    canal->leftGateDown = false;
    canal->rightGateDown = false;

    give_state_semaphore(canal);
}

static bool canal_add_task_at(Canal *canal, ShipTask *task, int index) {
    if (canal == NULL || task == NULL) {
        return false;
    }

    if (index < 0 || index >= canal->length) {
        return false;
    }

    if (canal->ships_inside.count >= CANAL_LEN) {
        return false;
    }

    if (canal->ships_inside.tasks[index] != NULL) {
        return false;
    }

    canal->ships_inside.tasks[index] = task;
    canal->ships_inside.count++;

    task->ship.position = index;

    return true;
}

static int get_entry_index(Canal *canal, ShipTask *task) {
    if (canal == NULL || task == NULL) {
        return -1;
    }

    if (task->ship.origin == LEFT) {
        return 0;
    }

    return canal->length - 1;
}

static int get_next_index(ShipTask *task) {
    if (task == NULL) {
        return -1;
    }

    if (task->ship.origin == LEFT) {
        return task->ship.position + 1;
    }

    return task->ship.position - 1;
}

static int find_task_position(Canal *canal, ShipTask *task) {
    if (canal == NULL || task == NULL) {
        return -1;
    }

    for (int i = 0; i < canal->length; i++) {
        if (canal->ships_inside.tasks[i] == task) {
            return i;
        }
    }

    return -1;
}

static bool take_state_semaphore(Canal *canal) {
    if (canal == NULL || canal->stateSemaphore == NULL) {
        return false;
    }

    return xSemaphoreTake(canal->stateSemaphore, portMAX_DELAY) == pdTRUE;
}

static void give_state_semaphore(Canal *canal) {
    if (canal == NULL || canal->stateSemaphore == NULL) {
        return;
    }

    xSemaphoreGive(canal->stateSemaphore);
}

static bool take_position_semaphore(Canal *canal, int index) {
    if (canal == NULL) {
        return false;
    }

    if (index < 0 || index >= CANAL_LEN) {
        return false;
    }

    if (canal->positionSemaphores[index] == NULL) {
        return false;
    }

    return xSemaphoreTake(canal->positionSemaphores[index], portMAX_DELAY) == pdTRUE;
}

static void give_position_semaphore(Canal *canal, int index) {
    if (canal == NULL) {
        return;
    }

    if (index < 0 || index >= CANAL_LEN) {
        return;
    }

    if (canal->positionSemaphores[index] == NULL) {
        return;
    }

    xSemaphoreGive(canal->positionSemaphores[index]);
}