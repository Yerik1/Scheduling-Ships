//
// Created by user on 4/27/2026.
//

#ifndef SHIP_FACTORY_H
#define SHIP_FACTORY_H
#include <stdlib.h>
#include <time.h>
#include "ship.h"

struct Ship createShip(ShipType shipType, Direction direction, int canalLength);
struct Ship createRndmShip(int canalLength);
struct Ship createRndmShipWDir(Direction dir, int canalLength);


#endif //SHIP_FACTORY_H
