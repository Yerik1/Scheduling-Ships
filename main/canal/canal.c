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

static int get_step_direction(ShipTask *task);
static int get_last_valid_index(Canal *canal, int nextIndex);
static bool canal_will_exit(Canal *canal, int nextIndex);

static bool take_path_semaphores(Canal *canal, int fromIndex, int toIndex);
static void give_path_semaphores(Canal *canal, int fromIndex, int toIndex);

static bool canal_path_is_free_locked(Canal *canal, ShipTask *task, int oldIndex, int endIndex);
static bool canal_is_blocked(Canal *canal);

void canal_init(Canal *canal, int length, Direction initial_direction)
{
    if (canal == NULL)
    {
        return;
    }

    if (length <= 0 || length > CANAL_LEN)
    {
        length = CANAL_LEN;
    }

    canal->length = length;
    canal->current_direction = initial_direction;

    canal->leftGateDown = false;
    canal->rightGateDown = false;
    canal->isBlocked = false;

    canal_list_init(&canal->ships_inside);

    canal->stateSemaphore = xSemaphoreCreateBinary();

    if (canal->stateSemaphore == NULL)
    {
        printf("ERROR: No se pudo crear stateSemaphore del canal\n");
        return;
    }

    xSemaphoreGive(canal->stateSemaphore);

    for (int i = 0; i < CANAL_LEN; i++)
    {
        canal->positionSemaphores[i] = xSemaphoreCreateBinary();

        if (canal->positionSemaphores[i] == NULL)
        {
            printf("ERROR: No se pudo crear semaforo para posicion %d\n", i);
        }
        else
        {
            xSemaphoreGive(canal->positionSemaphores[i]);
        }
    }
}

bool canal_is_empty(Canal *canal)
{
    if (canal == NULL)
    {
        return true;
    }

    if (!take_state_semaphore(canal))
    {
        return true;
    }

    bool result = canal_list_empty(&canal->ships_inside);

    give_state_semaphore(canal);

    return result;
}

bool canal_can_enter(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
    {
        return false;
    }

    int entryIndex = get_entry_index(canal, task);

    if (entryIndex < 0 || entryIndex >= canal->length)
    {
        return false;
    }

    bool result = false;

    if (!take_state_semaphore(canal))
    {
        return false;
    }

    if (!take_position_semaphore(canal, entryIndex))
    {
        give_state_semaphore(canal);
        return false;
    }

    if (canal->isBlocked)
    {
        result = false;
    }
    else if (task->ship.origin == LEFT && canal->leftGateDown)
    {
        result = false;
    }
    else if (task->ship.origin == RIGHT && canal->rightGateDown)
    {
        result = false;
    }
    else if (!canal_list_empty(&canal->ships_inside) &&
             canal->current_direction != task->ship.origin)
    {
        result = false;
    }
    else if (!is_pos_free(&canal->ships_inside, entryIndex))
    {
        result = false;
    }
    else
    {
        result = true;
    }

    give_position_semaphore(canal, entryIndex);
    give_state_semaphore(canal);

    return result;
}

bool canal_enter(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
    {
        return false;
    }

    int entryIndex = get_entry_index(canal, task);

    if (entryIndex < 0 || entryIndex >= canal->length)
    {
        return false;
    }

    bool result = false;

    if (!take_state_semaphore(canal))
    {
        return false;
    }

    if (!take_position_semaphore(canal, entryIndex))
    {
        give_state_semaphore(canal);
        return false;
    }

    if (canal->isBlocked)
    {
        result = false;
    }
    else if (task->ship.origin == LEFT && canal->leftGateDown)
    {
        result = false;
    }
    else if (task->ship.origin == RIGHT && canal->rightGateDown)
    {
        result = false;
    }
    else if (!canal_list_empty(&canal->ships_inside) &&
             canal->current_direction != task->ship.origin)
    {
        result = false;
    }
    else if (!is_pos_free(&canal->ships_inside, entryIndex))
    {
        result = false;
    }
    else
    {
        canal->current_direction = task->ship.origin;

        result = canal_add_task_at(canal, task, entryIndex);

        if (result)
        {
            task->ship.position = entryIndex;
            setState(&task->ship, RUNNING);

            printf(
                "Barco %d entro al canal en posicion %d\n",
                task->ship.id,
                entryIndex);
        }
    }

    give_position_semaphore(canal, entryIndex);
    give_state_semaphore(canal);

    return result;
}

bool canal_can_move_task(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
    {
        return false;
    }

    if (canal_is_blocked(canal))
    {
        return false;
    }

    int oldIndex = task->ship.position;

    if (oldIndex < 0 || oldIndex >= canal->length)
    {
        return false;
    }

    int nextIndex = get_next_index(task);
    int endIndex = get_last_valid_index(canal, nextIndex);

    if (endIndex < 0 || endIndex >= canal->length)
    {
        return false;
    }

    if (!take_path_semaphores(canal, oldIndex, endIndex))
    {
        return false;
    }

    bool result = canal_path_is_free_locked(canal, task, oldIndex, endIndex);

    give_path_semaphores(canal, oldIndex, endIndex);

    return result;
}

bool canal_move_task(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
    {
        return false;
    }

    if (canal_is_blocked(canal))
    {
        return false;
    }

    int oldIndex = task->ship.position;

    if (oldIndex < 0 || oldIndex >= canal->length)
    {
        return false;
    }

    int nextIndex = get_next_index(task);
    int endIndex = get_last_valid_index(canal, nextIndex);
    bool exits = canal_will_exit(canal, nextIndex);

    if (endIndex < 0 || endIndex >= canal->length)
    {
        return false;
    }

    if (!take_path_semaphores(canal, oldIndex, endIndex))
    {
        return false;
    }

    bool result = false;

    if (!canal_path_is_free_locked(canal, task, oldIndex, endIndex))
    {
        give_path_semaphores(canal, oldIndex, endIndex);
        return false;
    }

    if (exits)
    {
        ShipTask *removedTask = canal_list_remove(&canal->ships_inside, oldIndex);

        if (removedTask != NULL)
        {
            decRemainingTime(&removedTask->ship);
            finish(&removedTask->ship);

            printf(
                "Barco %d salio del canal desde posicion %d\n",
                removedTask->ship.id,
                oldIndex);

            result = true;
        }
    }
    else
    {
        result = move_task(&canal->ships_inside, oldIndex, nextIndex);

        if (result)
        {
            task->ship.position = nextIndex;
            decRemainingTime(&task->ship);

            printf(
                "Barco %d se movio de %d a %d\n",
                task->ship.id,
                oldIndex,
                nextIndex);
        }
    }

    give_path_semaphores(canal, oldIndex, endIndex);

    return result;
}

ShipTask *canal_remove_task(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
    {
        return NULL;
    }

    if (!take_state_semaphore(canal))
    {
        return NULL;
    }

    int index = find_task_position(canal, task);

    if (index == -1)
    {
        give_state_semaphore(canal);
        return NULL;
    }

    if (!take_position_semaphore(canal, index))
    {
        give_state_semaphore(canal);
        return NULL;
    }

    ShipTask *removedTask = canal_list_remove(&canal->ships_inside, index);

    if (removedTask != NULL)
    {
        rstPosition(&removedTask->ship);
        setState(&removedTask->ship, READY);

        printf(
            "Barco %d fue removido del canal y vuelve a READY\n",
            removedTask->ship.id);
    }

    give_position_semaphore(canal, index);
    give_state_semaphore(canal);

    return removedTask;
}

void canal_block(Canal *canal)
{
    if (canal == NULL)
    {
        return;
    }

    if (!take_state_semaphore(canal))
    {
        return;
    }

    canal->isBlocked = true;
    canal->leftGateDown = true;
    canal->rightGateDown = true;

    give_state_semaphore(canal);
}

void canal_unblock(Canal *canal)
{
    if (canal == NULL)
    {
        return;
    }

    if (!take_state_semaphore(canal))
    {
        return;
    }

    canal->isBlocked = false;
    canal->leftGateDown = false;
    canal->rightGateDown = false;

    give_state_semaphore(canal);
}

static bool canal_add_task_at(Canal *canal, ShipTask *task, int index)
{
    if (canal == NULL || task == NULL)
    {
        return false;
    }

    if (index < 0 || index >= canal->length)
    {
        return false;
    }

    if (canal->ships_inside.count >= CANAL_LEN)
    {
        return false;
    }

    if (canal->ships_inside.tasks[index] != NULL)
    {
        return false;
    }

    canal->ships_inside.tasks[index] = task;
    canal->ships_inside.count++;

    task->ship.position = index;

    return true;
}

static int get_entry_index(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
    {
        return -1;
    }

    if (task->ship.origin == LEFT)
    {
        return 0;
    }

    return canal->length - 1;
}

static int get_next_index(ShipTask *task)
{
    if (task == NULL)
    {
        return -1;
    }

    int speed = task->ship.speed;

    if (speed <= 0)
    {
        speed = 1;
    }

    if (task->ship.origin == LEFT)
    {
        return task->ship.position + speed;
    }

    return task->ship.position - speed;
}

static int find_task_position(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
    {
        return -1;
    }

    for (int i = 0; i < canal->length; i++)
    {
        if (canal->ships_inside.tasks[i] == task)
        {
            return i;
        }
    }

    return -1;
}

static bool take_state_semaphore(Canal *canal)
{
    if (canal == NULL || canal->stateSemaphore == NULL)
    {
        return false;
    }

    return xSemaphoreTake(canal->stateSemaphore, portMAX_DELAY) == pdTRUE;
}

static void give_state_semaphore(Canal *canal)
{
    if (canal == NULL || canal->stateSemaphore == NULL)
    {
        return;
    }

    xSemaphoreGive(canal->stateSemaphore);
}

static bool take_position_semaphore(Canal *canal, int index)
{
    if (canal == NULL)
    {
        return false;
    }

    if (index < 0 || index >= CANAL_LEN)
    {
        return false;
    }

    if (canal->positionSemaphores[index] == NULL)
    {
        return false;
    }

    return xSemaphoreTake(canal->positionSemaphores[index], portMAX_DELAY) == pdTRUE;
}

static void give_position_semaphore(Canal *canal, int index)
{
    if (canal == NULL)
    {
        return;
    }

    if (index < 0 || index >= CANAL_LEN)
    {
        return;
    }

    if (canal->positionSemaphores[index] == NULL)
    {
        return;
    }

    xSemaphoreGive(canal->positionSemaphores[index]);
}

static int get_step_direction(ShipTask *task)
{
    if (task == NULL)
    {
        return 0;
    }

    if (task->ship.origin == LEFT)
    {
        return 1;
    }

    return -1;
}

static bool canal_will_exit(Canal *canal, int nextIndex)
{
    if (canal == NULL)
    {
        return false;
    }

    return nextIndex < 0 || nextIndex >= canal->length;
}

static int get_last_valid_index(Canal *canal, int nextIndex)
{
    if (canal == NULL)
    {
        return -1;
    }

    if (nextIndex < 0)
    {
        return 0;
    }

    if (nextIndex >= canal->length)
    {
        return canal->length - 1;
    }

    return nextIndex;
}

static bool take_path_semaphores(Canal *canal, int fromIndex, int toIndex)
{
    if (canal == NULL)
    {
        return false;
    }

    if (fromIndex < 0 || fromIndex >= canal->length)
    {
        return false;
    }

    if (toIndex < 0 || toIndex >= canal->length)
    {
        return false;
    }

    int start = fromIndex < toIndex ? fromIndex : toIndex;
    int end = fromIndex < toIndex ? toIndex : fromIndex;

    for (int i = start; i <= end; i++)
    {
        if (!take_position_semaphore(canal, i))
        {
            for (int j = start; j < i; j++)
            {
                give_position_semaphore(canal, j);
            }

            return false;
        }
    }

    return true;
}

static void give_path_semaphores(Canal *canal, int fromIndex, int toIndex)
{
    if (canal == NULL)
    {
        return;
    }

    if (fromIndex < 0 || fromIndex >= canal->length)
    {
        return;
    }

    if (toIndex < 0 || toIndex >= canal->length)
    {
        return;
    }

    int start = fromIndex < toIndex ? fromIndex : toIndex;
    int end = fromIndex < toIndex ? toIndex : fromIndex;

    for (int i = end; i >= start; i--)
    {
        give_position_semaphore(canal, i);
    }
}

static bool canal_path_is_free_locked(Canal *canal, ShipTask *task, int oldIndex, int endIndex)
{
    if (canal == NULL || task == NULL)
    {
        return false;
    }

    if (oldIndex < 0 || oldIndex >= canal->length)
    {
        return false;
    }

    if (endIndex < 0 || endIndex >= canal->length)
    {
        return false;
    }

    if (canal->ships_inside.tasks[oldIndex] != task)
    {
        return false;
    }

    int step = get_step_direction(task);

    if (step == 0)
    {
        return false;
    }

    int current = oldIndex + step;

    while (true)
    {
        if (current < 0 || current >= canal->length)
        {
            break;
        }

        if (canal->ships_inside.tasks[current] != NULL)
        {
            return false;
        }

        if (current == endIndex)
        {
            break;
        }

        current += step;
    }

    return true;
}

static bool canal_is_blocked(Canal *canal)
{
    if (canal == NULL)
    {
        return true;
    }

    if (!take_state_semaphore(canal))
    {
        return true;
    }

    bool blocked = canal->isBlocked;

    give_state_semaphore(canal);

    return blocked;
}

bool canal_move_one_step(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
    {
        return false;
    }

    bool result = false;

    if (!take_state_semaphore(canal))
    {
        return false;
    }

    if (canal->isBlocked)
    {
        give_state_semaphore(canal);
        return false;
    }

    int oldIndex = find_task_position(canal, task);

    if (oldIndex == -1)
    {
        give_state_semaphore(canal);
        return false;
    }

    int nextIndex;

    if (task->ship.origin == LEFT)
    {
        nextIndex = oldIndex + 1;
    }
    else
    {
        nextIndex = oldIndex - 1;
    }

    /*
     * Si el siguiente índice sale del canal,
     * el barco termina su cruce.
     */
    if (nextIndex < 0 || nextIndex >= canal->length)
    {
        if (!take_position_semaphore(canal, oldIndex))
        {
            give_state_semaphore(canal);
            return false;
        }

        ShipTask *removedTask = canal_list_remove(&canal->ships_inside, oldIndex);

        if (removedTask != NULL)
        {
            finish(&removedTask->ship);

            printf(
                "Barco %d salio del canal desde posicion %d\n",
                removedTask->ship.id,
                oldIndex);

            result = true;
        }

        give_position_semaphore(canal, oldIndex);
        give_state_semaphore(canal);

        return result;
    }

    int firstLock = oldIndex < nextIndex ? oldIndex : nextIndex;
    int secondLock = oldIndex < nextIndex ? nextIndex : oldIndex;

    if (!take_position_semaphore(canal, firstLock))
    {
        give_state_semaphore(canal);
        return false;
    }

    if (!take_position_semaphore(canal, secondLock))
    {
        give_position_semaphore(canal, firstLock);
        give_state_semaphore(canal);
        return false;
    }

    /*
     * Solo se mueve si la siguiente casilla está libre.
     * Si está ocupada, se queda justo antes.
     */
    if (is_pos_free(&canal->ships_inside, nextIndex))
    {
        result = move_task(&canal->ships_inside, oldIndex, nextIndex);

        if (result)
        {
            task->ship.position = nextIndex;
            decRemainingTime(&task->ship);

            printf(
                "Barco %d avanzo de %d a %d\n",
                task->ship.id,
                oldIndex,
                nextIndex);
        }
    }
    else
    {
        printf(
            "Barco %d bloqueado en %d, siguiente posicion ocupada: %d\n",
            task->ship.id,
            oldIndex,
            nextIndex);
    }

    give_position_semaphore(canal, secondLock);
    give_position_semaphore(canal, firstLock);
    give_state_semaphore(canal);

    return result;
}

void canal_notify_ships(Canal *canal)
{
    if (canal == NULL)
    {
        return;
    }

    if (!take_state_semaphore(canal))
    {
        return;
    }

    for (int i = 0; i < canal->length; i++)
    {
        ShipTask *task = canal->ships_inside.tasks[i];

        if (task != NULL && task->handle != NULL)
        {
            xTaskNotifyGive(task->handle);
        }
    }

    give_state_semaphore(canal);
}

void canal_notify_ships_ordered(Canal *canal)
{
    if (canal == NULL)
    {
        return;
    }

    /*
     * Si el flujo va de izquierda a derecha, los barcos avanzan hacia índices mayores.
     * Entonces se debe notificar primero el índice más alto.
     */
    if (canal->current_direction == LEFT)
    {
        for (int i = canal->length - 1; i >= 0; i--)
        {
            ShipTask *task = canal->ships_inside.tasks[i];

            if (task != NULL && task->handle != NULL)
            {
                xTaskNotifyGive(task->handle);

                /*
                 * Pequeño margen para que el barco de adelante libere espacio
                 * antes de despertar al que viene detrás.
                 */
                vTaskDelay(pdMS_TO_TICKS(30));
            }
        }
    }

    /*
     * Si el flujo va de derecha a izquierda, los barcos avanzan hacia índices menores.
     * Entonces se debe notificar primero el índice más bajo.
     */
    else if (canal->current_direction == RIGHT)
    {
        for (int i = 0; i < canal->length; i++)
        {
            ShipTask *task = canal->ships_inside.tasks[i];

            if (task != NULL && task->handle != NULL)
            {
                xTaskNotifyGive(task->handle);
                vTaskDelay(pdMS_TO_TICKS(30));
            }
        }
    }
}

void canal_set_ship_position(Canal *canal, ShipTask *task, int position)
{
    if (canal == NULL || task == NULL)
        return;

    if (position < 0 || position >= canal->length)
        return;

    // Buscar la celda actual del barco y limpiarlo
    for (int i = 0; i < canal->length; i++)
    {
        if (canal->ships_inside.tasks[i] == task)
        {
            canal->ships_inside.tasks[i] = NULL;
            break;
        }
    }

    // Colocarlo en la posición restaurada
    // Si la celda está ocupada, buscar la más cercana disponible
    if (canal->ships_inside.tasks[position] == NULL)
    {
        canal->ships_inside.tasks[position] = task;
        task->ship.position = position;
    }
    else
    {
        // Buscar celda libre más cercana hacia atrás (dirección origen)
        for (int offset = 1; offset < canal->length; offset++)
        {
            int fallback = position - offset;
            if (fallback < 0)
                break;
            if (canal->ships_inside.tasks[fallback] == NULL)
            {
                canal->ships_inside.tasks[fallback] = task;
                task->ship.position = fallback;
                printf(
                    "WARN: posicion %d ocupada, barco %d colocado en %d.\n",
                    position, task->ship.id, fallback);
                break;
            }
        }
    }
}

bool canal_enter_at(Canal *canal, ShipTask *task, int position)
{
    if (canal == NULL || task == NULL)
        return false;

    if (position < 0 || position >= canal->length)
        return false;

    if (!take_state_semaphore(canal))
        return false;

    if (!take_position_semaphore(canal, position))
    {
        give_state_semaphore(canal);
        return false;
    }

    bool result = false;

    if (!canal->isBlocked && canal->ships_inside.tasks[position] == NULL)
    {
        result = canal_add_task_at(canal, task, position);

        if (result)
        {
            canal->current_direction = task->ship.origin;
            task->ship.position = position;
            setState(&task->ship, RUNNING);

            printf(
                "Barco %d re-ingreso al canal en checkpoint posicion %d\n",
                task->ship.id,
                position);
        }
    }
    else
    {
        // Posición ocupada: buscar celda libre más cercana hacia el origen
        int step = (task->ship.origin == LEFT) ? -1 : 1;

        give_position_semaphore(canal, position);

        for (int offset = 1; offset < canal->length; offset++)
        {
            int fallback = position + (step * offset);

            if (fallback < 0 || fallback >= canal->length)
                break;

            if (!take_position_semaphore(canal, fallback))
                continue;

            if (!canal->isBlocked && canal->ships_inside.tasks[fallback] == NULL)
            {
                result = canal_add_task_at(canal, task, fallback);

                if (result)
                {
                    canal->current_direction = task->ship.origin;
                    task->ship.position = fallback;
                    setState(&task->ship, RUNNING);

                    printf(
                        "WARN: checkpoint %d ocupado, barco %d re-ingreso en %d\n",
                        position, task->ship.id, fallback);
                }

                give_position_semaphore(canal, fallback);
                break;
            }

            give_position_semaphore(canal, fallback);
        }

        give_state_semaphore(canal);
        return result;
    }

    give_position_semaphore(canal, position);
    give_state_semaphore(canal);

    return result;
}
