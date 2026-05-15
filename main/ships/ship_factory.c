#include "ship_factory.h"

// File-scoped static counter to generate unique sequential IDs
static int ship_counter = 0;

/* ========================================================================== */
/* FACTORY CREATION FUNCTIONS                                                 */
/* ========================================================================== */

Ship ship_factory_create(ShipType ship_type, Direction direction, int canal_length)
{
    int id = ship_counter++;
    ShipState ship_state = READY;
    int position = 0;

    int speed;
    int priority;

    // Set directional vectors based on the entry direction
    Direction origin = (direction == LEFT) ? LEFT : RIGHT;
    Direction destination = (direction == LEFT) ? RIGHT : LEFT;

    // Configure ship metrics based on its specific type
    switch (ship_type)
    {
    case NORMAL:
        speed = 1;
        priority = 3;
        break;
    case FISHING:
        speed = 2;
        priority = 2;
        break;
    default: // e.g., PATROL or other types
        speed = 3;
        priority = 1;
        break;
    }

    // Ceiling division formula to calculate exact ticks needed to clear the canal:
    // burst_time = ceil(canal_length / speed)
    int burst_time = (canal_length + speed - 1) / speed;
    int remaining_time = burst_time;
    int deadline = burst_time * priority;

    return ship_init(id, ship_type, origin, destination, ship_state, priority,
                     burst_time, remaining_time, deadline, speed, position);
}

Ship ship_factory_create_random(int canal_length)
{
    // Note: Ensure srand() has been called once in main() before using this function
    Direction random_dir = (Direction)(rand() % (RIGHT + 1));
    ShipType random_type = (ShipType)(rand() % (PATROL + 1));

    return ship_factory_create(random_type, random_dir, canal_length);
}

Ship ship_factory_create_random_with_dir(Direction dir, int canal_length)
{
    ShipType random_type = (ShipType)(rand() % (PATROL + 1));

    return ship_factory_create(random_type, dir, canal_length);
}
