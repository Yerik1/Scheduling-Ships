#ifndef SHIP_FACTORY_H
#define SHIP_FACTORY_H

#include <stdlib.h>
#include <time.h>
#include "ship.h"

/* ========================================================================== */
/* FACTORY CREATION FUNCTIONS                                                 */
/* ========================================================================== */

/**
 * @brief Creates a standardized Ship instance based on specific type and direction.
 * @param ship_type The category of the ship (e.g., Cargo, Passenger, etc.).
 * @param direction The orientation/entry point of the ship.
 * @param canal_length The total length of the canal to calculate metrics like deadline.
 * @return Ship A fully initialized Ship structure.
 */
Ship ship_factory_create(ShipType ship_type, Direction direction, int canal_length);

/**
 * @brief Generates a completely randomized Ship with pseudo-random attributes.
 * @note Requires the random number generator (srand) to be seeded beforehand.
 */
Ship ship_factory_create_random(int canal_length);

/**
 * @brief Generates a randomized Ship but enforces a specific entry direction.
 */
Ship ship_factory_create_random_with_dir(Direction dir, int canal_length);

#endif // SHIP_FACTORY_H
