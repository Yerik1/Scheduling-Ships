#ifndef SHIP_H
#define SHIP_H

#include <stddef.h>
#include "../common/types.h"

/**
 * @brief Structure representing a ship entity and its scheduling metrics.
 */
typedef struct
{
    int id;
    ShipType type;
    Direction origin;
    Direction destination;
    ShipState state;
    int priority;
    int burst_time;     // Total time required to traverse the canal
    int remaining_time; // Time left to finish traversal
    int deadline;       // Time limit before penalty or failure
    int speed;
    int position; // Current index position inside the canal
} Ship;

/* ========================================================================== */
/* INITIALIZATION AND GETTERS                                                 */
/* ========================================================================== */

/**
 * @brief Initializes and returns a new Ship instance with the given parameters.
 */
Ship ship_init(int id, ShipType type, Direction origin, Direction destination, ShipState state,
               int priority, int burst_time, int remaining_time, int deadline, int speed, int position);

/**
 * @brief Safely retrieves the current speed of the ship.
 */
int ship_get_speed(const Ship *ship);

/* ========================================================================== */
/* STATE AND POSITION MODIFIERS                                               */
/* ========================================================================== */

/**
 * @brief Updates the lifecycle state of the ship.
 */
void ship_set_state(Ship *ship, ShipState state);

/**
 * @brief Updates/advances the ship's position based on its direction or speed.
 */
void ship_update_position(Ship *ship);

/**
 * @brief Resets the ship's position to its default out-of-canal state.
 */
void ship_reset_position(Ship *ship);

/**
 * @brief Decrements the remaining time of the ship by one simulation tick.
 */
void ship_decrement_remaining_time(Ship *ship);

/**
 * @brief Finalizes the ship task, moving its state to finished.
 */
void ship_finish(Ship *ship);

#endif // SHIP_H
