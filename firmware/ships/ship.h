//
// Created by user on 4/27/2026.
//

#ifndef SHIP_H
#define SHIP_H
#include <stddef.h>
#include "../common/types.h"

struct Ship{
    int id;
    ShipType type;
    Direction origin;
    Direction destination;
    ShipState state;
    int priority;
    int burstTime;
    int remainingTime;
    int deadline;
    int speed;
    int position;
};

struct Ship initShip(int id, ShipType type, Direction origin, Direction destination, ShipState state,
                        int priority, int burstTime, int remainingTime, int deadline, int speed, int position);
void setState(struct Ship *ship, ShipState state);
void updtPosition(struct Ship *ship);
void rstPosition(struct Ship *ship);
void decRemainingTime(struct Ship *ship);
void finish(struct Ship *ship);
int getSpeed(const struct Ship *ship);


#endif //SHIP_H
