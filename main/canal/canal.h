#ifndef CANAL_H
#define CANAL_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../queues/canal_list.h"

/**
 * @brief Structure representing the shared canal resource, guarded by semaphores.
 */
typedef struct
{
    int length;
    Direction current_direction;
    CanalList ships_inside;
    bool left_gate_down;  // True if the left entrance gate is closed
    bool right_gate_down; // True if the right entrance gate is closed
    bool is_blocked;      // Emergency block flag for the entire canal

    SemaphoreHandle_t state_semaphore;                // Mutex for guarding general canal state variables
    SemaphoreHandle_t position_semaphores[CANAL_LEN]; // Binary semaphores for tracking slot occupancy
} Canal;

/* ========================================================================== */
/* LIFECYCLE AND LIFESPAN MANAGEMENT                                          */
/* ========================================================================== */

/**
 * @brief Initializes the canal properties, gates, and creates FreeRTOS semaphores.
 */
void canal_init(Canal *canal, int length, Direction initial_direction);

/**
 * @brief Thread-safely checks if there are no ships inside the canal.
 */
bool canal_is_empty(Canal *canal);

/* ========================================================================== */
/* FLOW CONTROL AND ENTRY EVALUATION                                          */
/* ========================================================================== */

/**
 * @brief Validates if a ship is legally allowed to enter the canal based on current state.
 */
bool canal_can_enter(Canal *canal, ShipTask *task);

/**
 * @brief Places a ship task inside the canal at its designated entry point.
 * @return true if successful, false if blocked or position occupied.
 */
bool canal_enter(Canal *canal, ShipTask *task);

/**
 * @brief Forces/allows entry at a specific position instead of default direction rules.
 */
bool canal_enter_at(Canal *canal, ShipTask *task, int position);

/* ========================================================================== */
/* MOVEMENT AND POSITION MODIFICATIONS                                        */
/* ========================================================================== */

/**
 * @brief Evaluates if the next position in the ship's path is clear and unblocked.
 */
bool canal_can_move_task(Canal *canal, ShipTask *task);

/**
 * @brief Moves the ship task to its next position index, handling semaphore handshakes.
 */
bool canal_move_task(Canal *canal, ShipTask *task);

/**
 * @brief Automatically calculates and advances the ship forward by one index step.
 */
bool canal_move_one_step(Canal *canal, ShipTask *task);

/**
 * @brief Manually updates the target ship's position tracking field.
 */
void canal_set_ship_position(Canal *canal, ShipTask *task, int position);

/**
 * @brief Removes a ship from the canal list completely (e.g., when exiting).
 * @return ShipTask* Pointer to the removed task.
 */
ShipTask *canal_remove_task(Canal *canal, ShipTask *task);

/* ========================================================================== */
/* BLOCKING AND SYNCHRONIZATION (FreeRTOS Signaling)                         */
/* ========================================================================== */

/**
 * @brief Blocks the canal entirely, stopping new inputs.
 */
void canal_block(Canal *canal);

/**
 * @brief Lifts the block flag, restoring standard canal operations.
 */
void canal_unblock(Canal *canal);

/**
 * @brief Signals/notifies waiting ship threads that canal state changed.
 */
void canal_notify_ships(Canal *canal);

/**
 * @brief Signals/notifies waiting ship threads following a strict scheduling order.
 */
void canal_notify_ships_ordered(Canal *canal);

#endif // CANAL_H
