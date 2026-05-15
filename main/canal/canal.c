#include "canal.h"
#include <stdio.h>

/* ========================================================================== */
/* PRIVATE/STATIC FUNCTION PROTOTYPES                                         */
/* ========================================================================== */

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
static bool canal_is_blocked_internal(Canal *canal);
static bool canal_validate_entry_conditions_locked(const Canal *canal, const ShipTask *task, int entry_index);

/* ========================================================================== */
/* LIFECYCLE AND LIFESPAN MANAGEMENT                                          */
/* ========================================================================== */

void canal_init(Canal *canal, int length, Direction initial_direction)
{
    if (canal == NULL)
        return;

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
        printf("ERROR: Could not create canal state_semaphore\n");
        return;
    }

    xSemaphoreGive(canal->stateSemaphore);

    for (int i = 0; i < CANAL_LEN; i++)
    {
        canal->positionSemaphores[i] = xSemaphoreCreateBinary();
        if (canal->positionSemaphores[i] == NULL)
        {
            printf("ERROR: Could not create semaphore for position %d\n", i);
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
        return true;

    // Cast away const carefully only for the internal synchronization step
    Canal *mutable_canal = (Canal *)canal;
    if (!take_state_semaphore(mutable_canal))
        return true;

    bool result = canal_list_empty(&canal->ships_inside);
    give_state_semaphore(canal);

    return result;
}

/* ========================================================================== */
/* FLOW CONTROL AND ENTRY EVALUATION                                          */
/* ========================================================================== */

bool canal_can_enter(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
        return false;

    int entry_index = get_entry_index(canal, task);
    if (entry_index < 0 || entry_index >= canal->length)
        return false;

    Canal *mutable_canal = (Canal *)canal;
    if (!take_state_semaphore(mutable_canal))
        return false;

    if (!take_position_semaphore(mutable_canal, entry_index))
    {
        give_state_semaphore(mutable_canal);
        return false;
    }

    bool result = canal_validate_entry_conditions_locked(canal, task, entry_index);

    give_position_semaphore(mutable_canal, entryIndex);
    give_state_semaphore(mutable_canal);

    return result;
}

bool canal_enter(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
        return false;

    int entry_index = get_entry_index(canal, task);
    if (entry_index < 0 || entry_index >= canal->length)
        return false;

    if (!take_state_semaphore(canal))
        return false;

    if (!take_position_semaphore(canal, entry_index))
    {
        give_state_semaphore(canal);
        return false;
    }

    bool result = false;
    if (canal_validate_entry_conditions_locked(canal, task, entry_index))
    {
        canal->current_direction = task->ship.origin;
        result = canal_add_task_at(canal, task, entry_index);

        if (result)
        {
            task->ship.position = entry_index;
            ship_set_state(&task->ship, RUNNING);
            printf("Ship %d entered canal at position %d\n", task->ship.id, entry_index);
        }
    }

    give_position_semaphore(canal, entry_index);
    give_state_semaphore(canal);

    return result;
}

bool canal_enter_at(Canal *canal, ShipTask *task, int target_index)
{
    if (canal == NULL || task == NULL || target_index < 0 || target_index >= canal->length)
        return false;

    if (!take_state_semaphore(canal))
        return false;

    if (canal->is_blocked)
    {
        give_state_semaphore(canal);
        return false;
    }

    if (!take_position_semaphore(canal, target_index))
    {
        give_state_semaphore(canal);
        return false;
    }

    if (!is_pos_free(&canal->ships_inside, target_index))
    {
        give_position_semaphore(canal, target_index);
        give_state_semaphore(canal);
        return false;
    }

    canal->ships_inside.tasks[target_index] = task;
    canal->ships_inside.count++;
    task->ship.position = target_index;
    ship_set_state(&task->ship, RUNNING);
    canal->current_direction = task->ship.origin;

    give_position_semaphore(canal, target_index);
    give_state_semaphore(canal);

    return true;
}

/* ========================================================================== */
/* MOVEMENT AND POSITION MODIFICATIONS                                        */
/* ========================================================================== */

bool canal_can_move_task(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
        return false;

    Canal *mutable_canal = (Canal *)canal;
    if (canal_is_blocked_internal(mutable_canal))
        return false;

    int old_index = task->ship.position;
    if (old_index < 0 || old_index >= canal->length)
        return false;

    int next_index = get_next_index(task);
    int end_index = get_last_valid_index(canal, next_index);
    if (end_index < 0 || end_index >= canal->length)
        return false;

    if (!take_path_semaphores(mutable_canal, old_index, end_index))
        return false;

    bool result = canal_path_is_free_locked(canal, task, old_index, end_index);
    give_path_semaphores(mutable_canal, old_index, end_index);

    return result;
}

bool canal_move_task(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
        return false;
    if (canal_is_blocked_internal(canal))
        return false;

    int old_index = task->ship.position;
    if (old_index < 0 || old_index >= canal->length)
        return false;

    int next_index = get_next_index(task);
    int end_index = get_last_valid_index(canal, next_index);
    bool exits = canal_will_exit(canal, next_index);

    if (end_index < 0 || end_index >= canal->length)
        return false;

    if (!take_path_semaphores(canal, old_index, end_index))
        return false;

    if (!canal_path_is_free_locked(canal, task, old_index, end_index))
    {
        give_path_semaphores(canal, old_index, end_index);
        return false;
    }

    bool result = false;

    if (exits)
    {
        ShipTask *removed_task = canal_list_remove(&canal->ships_inside, old_index);
        if (removed_task != NULL)
        {
            ship_decrement_remaining_time(&removed_task->ship);
            ship_finish(&removed_task->ship);
            printf("Ship %d exited canal from position %d\n", removed_task->ship.id, old_index);
            result = true;
        }
    }
    else
    {
        result = move_task(&canal->ships_inside, old_index, next_index);
        if (result)
        {
            task->ship.position = next_index;
            ship_decrement_remaining_time(&task->ship);
            printf("Ship %d moved from %d to %d\n", task->ship.id, old_index, next_index);
        }
    }

    give_path_semaphores(canal, old_index, end_index);
    return result;
}

bool canal_move_one_step(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
        return false;

    if (!take_state_semaphore(canal))
        return false;

    if (canal->is_blocked)
    {
        give_state_semaphore(canal);
        return false;
    }

    int old_index = find_task_position(canal, task);
    if (old_index == -1)
    {
        give_state_semaphore(canal);
        return false;
    }

    int next_index = (task->ship.origin == LEFT) ? (old_index + 1) : (old_index - 1);
    bool result = false;

    // Handle immediate exit scenario (moving out of canal limits)
    if (nextIndex < 0 || nextIndex >= canal->length)
    {
        if (!take_position_semaphore(canal, old_index))
        {
            give_state_semaphore(canal);
            return false;
        }

        ShipTask *removed_task = canal_list_remove(&canal->ships_inside, old_index);
        if (removed_task != NULL)
        {
            ship_finish(&removed_task->ship);
            printf("Ship %d exited canal from position %d\n", removed_task->ship.id, old_index);
            result = true;
        }

        give_position_semaphore(canal, old_index);
        give_state_semaphore(canal);
        return result;
    }

    // Determine lock hierarchy to prevent deadlocks (always lock smaller index first)
    int first_lock = (old_index < next_index) ? old_index : next_index;
    int second_lock = (old_index < next_index) ? next_index : old_index;

    if (!take_position_semaphore(canal, first_lock))
    {
        give_state_semaphore(canal);
        return false;
    }
    if (!take_position_semaphore(canal, second_lock))
    {
        give_position_semaphore(canal, first_lock);
        give_state_semaphore(canal);
        return false;
    }

    if (is_pos_free(&canal->ships_inside, next_index))
    {
        result = move_task(&canal->ships_inside, old_index, next_index);
        if (result)
        {
            task->ship.position = next_index;
            ship_decrement_remaining_time(&task->ship);
            printf("Ship %d advanced from %d to %d\n", task->ship.id, old_index, next_index);
        }
    }
    else
    {
        printf("Ship %d blocked at %d, next position occupied: %d\n", task->ship.id, old_index, next_index);
    }

    give_position_semaphore(canal, second_lock);
    give_position_semaphore(canal, first_lock);
    give_state_semaphore(canal);

    return result;
}

void canal_set_ship_position(Canal *canal, ShipTask *task, int position)
{
    if (canal == NULL || task == NULL || position < 0 || position >= canal->length)
        return;

    for (int i = 0; i < canal->length; i++)
    {
        if (canal->ships_inside.tasks[i] == task)
        {
            canal->ships_inside.tasks[i] = NULL;
            break;
        }
    }

    if (canal->ships_inside.tasks[position] == NULL)
    {
        canal->ships_inside.tasks[position] = task;
        task->ship.position = position;
    }
    else
    {
        // Fallback strategy: find the closest free cell backward
        for (int offset = 1; offset < canal->length; offset++)
        {
            int fallback = position - offset;
            if (fallback < 0)
                break;

            if (canal->ships_inside.tasks[fallback] == NULL)
            {
                canal->ships_inside.tasks[fallback] = task;
                task->ship.position = fallback;
                printf("WARN: Position %d occupied, placing ship %d at fallback %d.\n", position, task->ship.id, fallback);
                break;
            }
        }
    }
}

ShipTask *canal_remove_task(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
        return NULL;

    if (!take_state_semaphore(canal))
        return NULL;

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

    ShipTask *removed_task = canal_list_remove(&canal->ships_inside, index);
    if (removed_task != NULL)
    {
        ship_reset_position(&removed_task->ship);
        ship_set_state(&removed_task->ship, READY);
        printf("Ship %d was removed from canal and returned to READY\n", removed_task->ship.id);
    }

    give_position_semaphore(canal, index);
    give_state_semaphore(canal);

    return removed_task;
}

/* ========================================================================== */
/* BLOCKING AND SYNCHRONIZATION (FreeRTOS Signaling)                         */
/* ========================================================================== */

void canal_block(Canal *canal)
{
    if (canal == NULL)
        return;

    if (take_state_semaphore(canal))
    {
        canal->is_blocked = true;
        canal->left_gate_down = true;
        canal->right_gate_down = true;
        give_state_semaphore(canal);
    }
}

void canal_unblock(Canal *canal)
{
    if (canal == NULL)
        return;

    if (take_state_semaphore(canal))
    {
        canal->is_blocked = false;
        canal->left_gate_down = false;
        canal->right_gate_down = false;
        give_state_semaphore(canal);
    }
}

void canal_notify_ships(Canal *canal)
{
    if (canal == NULL)
        return;

    if (!take_state_semaphore(canal))
        return;

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
        return;

    // Wake up ships from exit to entry to smoothly clear the path ahead
    if (canal->current_direction == LEFT)
    {
        for (int i = canal->length - 1; i >= 0; i--)
        {
            ShipTask *task = canal->ships_inside.tasks[i];
            if (task != NULL && task->handle != NULL)
            {
                xTaskNotifyGive(task->handle);
                vTaskDelay(pdMS_TO_TICKS(30)); // Small yield margin to prevent race bursts
            }
        }
    }
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

/* ========================================================================== */
/* PRIVATE/STATIC IMPLEMENTATIONS                                             */
/* ========================================================================== */

static int get_entry_index(const Canal *canal, const ShipTask *task)
{
    if (canal == NULL || task == NULL)
        return -1;
    return (task->ship.origin == LEFT) ? 0 : (canal->length - 1);
}

static int get_next_index(const ShipTask *task)
{
    if (task == NULL)
        return -1;
    int speed = (task->ship.speed <= 0) ? 1 : task->ship.speed;
    return (task->ship.origin == LEFT) ? (task->ship.position + speed) : (task->ship.position - speed);
}

static int find_task_position(Canal *canal, ShipTask *task)
{
    if (canal == NULL || task == NULL)
        return -1;

    for (int i = 0; i < canal->length; i++)
    {
        if (canal->ships_inside.tasks[i] == task)
            return i;
    }
    return -1;
}

static bool take_state_semaphore(Canal *canal)
{
    if (canal == NULL || canal->state_semaphore == NULL)
        return false;
    return xSemaphoreTake(canal->state_semaphore, portMAX_DELAY) == pdTRUE;
}

static void give_state_semaphore(Canal *canal)
{
    if (canal != NULL && canal->state_semaphore != NULL)
    {
        xSemaphoreGive(canal->state_semaphore);
    }
}

static bool take_position_semaphore(Canal *canal, int index)
{
    if (canal == NULL || index < 0 || index >= CANAL_LEN || canal->position_semaphores[index] == NULL)
        return false;
    return xSemaphoreTake(canal->position_semaphores[index], portMAX_DELAY) == pdTRUE;
}

static void give_position_semaphore(Canal *canal, int index)
{
    if (canal != NULL && index >= 0 && index < CANAL_LEN && canal->position_semaphores[index] != NULL)
    {
        xSemaphoreGive(canal->position_semaphores[index]);
    }
}

static bool canal_add_task_at(Canal *canal, ShipTask *task, int index)
{
    if (canal == NULL || task == NULL || index < 0 || index >= canal->length)
        return false;
    if (canal->ships_inside.count >= CANAL_LEN || canal->ships_inside.tasks[index] != NULL)
        return false;

    canal->ships_inside.tasks[index] = task;
    canal->ships_inside.count++;
    task->ship.position = index;

    return true;
}

static int get_step_direction(ShipTask *task)
{
    if (task == NULL)
        return 0;
    return (task->ship.origin == LEFT) ? 1 : -1;
}

static int get_last_valid_index(Canal *canal, int next_index)
{
    if (canal == NULL)
        return -1;
    if (next_index < 0)
        return 0;
    if (next_index >= canal->length)
        return canal->length - 1;
    return next_index;
}

static bool canal_will_exit(Canal *canal, int next_index)
{
    if (canal == NULL)
        return false;
    return next_index < 0 || next_index >= canal->length;
}

static bool take_path_semaphores(Canal *canal, int from_index, int to_index)
{
    if (canal == NULL || from_index < 0 || from_index >= canal->length || to_index < 0 || to_index >= canal->length)
        return false;

    int start = (from_index < to_index) ? from_index : to_index;
    int end = (from_index < to_index) ? to_index : from_index;

    // Ordered Locking to prevent classic dining-philosophers deadlock conditions
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

static void give_path_semaphores(Canal *canal, int from_index, int to_index)
{
    if (canal == NULL || from_index < 0 || from_index >= canal->length || to_index < 0 || to_index >= canal->length)
        return;

    int start = (from_index < to_index) ? from_index : to_index;
    int end = (from_index < to_index) ? to_index : from_index;

    // Release locks in reverse order
    for (int i = end; i >= start; i--)
    {
        give_position_semaphore(canal, i);
    }
}

static bool canal_path_is_free_locked(Canal *canal, ShipTask *task, int old_index, int end_index)
{
    if (canal == NULL || task == NULL || old_index < 0 || old_index >= canal->length || end_index < 0 || end_index >= canal->length)
        return false;
    if (canal->ships_inside.tasks[old_index] != task)
        return false;

    int step = get_step_direction(task);
    if (step == 0)
        return false;

    int current = old_index + step;
    while (true)
    {
        if (current < 0 || current >= canal->length)
            break;
        if (canal->ships_inside.tasks[current] != NULL)
            return false;
        if (current == end_index)
            break;

        current += step;
    }
    return true;
}

static bool canal_is_blocked_internal(Canal *canal)
{
    if (canal == NULL)
        return true;
    if (!take_state_semaphore(canal))
        return true;

    bool blocked = canal->is_blocked;
    give_state_semaphore(canal);
    return blocked;
}

static bool canal_validate_entry_conditions_locked(const Canal *canal, const ShipTask *task, int entry_index)
{
    if (canal->is_blocked)
        return false;
    if (task->ship.origin == LEFT && canal->left_gate_down)
        return false;
    if (task->ship.origin == RIGHT && canal->right_gate_down)
        return false;

    // Direction validation: Block if canal is processing inverse transit traffic
    if (!canal_list_empty(&canal->ships_inside) && canal->current_direction != task->ship.origin)
    {
        return false;
    }

    return is_pos_free(&canal->ships_inside, entry_index);
}
