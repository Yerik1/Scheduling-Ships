//
// Created by user on 4/27/2026.
//

#include "ship.h"



struct Ship initShip(int id, ShipType type, Direction origin, Direction destination, ShipState state,
                        int priority, int burstTime, int remainingTime, int deadline, int speed, int position) {
    struct Ship ship;
    ship.id = id;
    ship.type = type;
    ship.origin = origin;
    ship.destination = destination;
    ship.state = state;
    ship.priority = priority;
    ship.burstTime = burstTime;
    ship.remainingTime = remainingTime;
    ship.deadline = deadline;
    ship.speed = speed;
    ship.position = position;

    return ship;
}

void setState(struct Ship *ship, ShipState state) {
    if (ship == NULL) return;
    ship->state = state;
}

void updtPosition(struct Ship *ship) {
    ship->position = ship->position + ship->speed ;
}

void rstPosition(struct Ship *ship) {
    ship->position = 0 ;
}

void decRemainingTime(struct Ship *ship) {
    if (ship->remainingTime > 0) {
        ship->remainingTime--;
    }
}

void finish(struct Ship *ship) {
    if (ship == NULL) return;
    ship->state = FINISHED;
}

int getSpeed(const struct Ship *ship) {
    if (ship == NULL) return 0;
    return ship->speed;
}