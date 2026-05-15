#include "ship.h"

/* ========================================================================== */
/* INITIALIZATION AND GETTERS                                                 */
/* ========================================================================== */

Ship ship_init(int id, ShipType type, Direction origin, Direction destination, ShipState state,
               int priority, int burst_time, int remaining_time, int deadline, int speed, int position)
{
    // Initialize structure directly using designated initializers
    Ship ship = {
        .id = id,
        .type = type,
        .origin = origin,
        .destination = destination,
        .state = state,
        .priority = priority,
        .burst_time = burst_time,
        .remaining_time = remaining_time,
        .deadline = deadline,
        .speed = speed,
        .position = position};

    return ship;
}

int ship_get_speed(const Ship *ship)
{
    if (ship == NULL)
        return 0;
    return ship->speed;
}

/* ========================================================================== */
/* STATE AND POSITION MODIFIERS                                               */
/* ========================================================================== */

void ship_set_state(Ship *ship, ShipState state)
{
    if (ship == NULL)
        return;
    ship->state = state;
}

void ship_update_position(Ship *ship)
{
    if (ship == NULL)
        return;

    // Advance position based on the ship's speed attribute
    ship->position += ship->speed;
}

void ship_reset_position(Ship *ship)
{
    if (ship == NULL)
        return;
    ship->position = 0;
}

void ship_decrement_remaining_time(Ship *ship)
{
    if (ship == NULL)
        return;

    // Decrement only if there is time remaining to avoid negative values
    if (ship->remaining_time > 0)
    {
        ship->remaining_time--;
    }
}

void ship_finish(Ship *ship)
{
    if (ship == NULL)
        return;
    ship->state = FINISHED;
}
