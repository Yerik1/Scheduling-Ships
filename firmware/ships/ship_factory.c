//
// Created by user on 4/27/2026.
//

#include "ship_factory.h"
static int contador = 0;

struct Ship createShip(ShipType shipType, Direction direction, int canalLength) {
    int id = contador;
    contador++;
    ShipState shipState = READY;
    Direction origin;
    Direction destination;
    int speed;
    int priority;
    int position = 0;
    int burstTime;
    int remainingTime;
    int deadline;

    if (direction == LEFT) {
        origin = LEFT;
        destination = RIGHT;
    }else {
        origin = RIGHT;
        destination = LEFT;
    }
    if (shipType == NORMAL) {
        speed = 1;
        priority = 3;
    }else if (shipType == FISHING) {
        speed = 2;
        priority = 2;
    }else{
        speed = 3;
        priority = 1;
    }

    burstTime = (canalLength + speed - 1) / speed;
    remainingTime = burstTime;
    deadline = burstTime * priority;

    return initShip(id, shipType, origin, destination, shipState, priority,
        burstTime, remainingTime, deadline, speed,position);
}

struct Ship createRndmShip(int canalLength) {
    srand(time(NULL));
    Direction dir = rand() % (RIGHT + 1);
    srand(time(NULL));
    ShipType type = rand() % (PATROL + 1);

    return createShip(type, dir, canalLength);

}

struct Ship createRndmShipWDir(Direction dir, int canalLength) {
    ShipType type = rand() % (PATROL + 1);

    return createShip(type, dir, canalLength);
}