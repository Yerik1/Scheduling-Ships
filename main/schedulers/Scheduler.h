#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stddef.h>
#include "../ships/ship.h"

#define MAX_SHIPS 64

// Ready Queue struct
typedef struct
{
    struct Ship ships[MAX_SHIPS];
    int count;
} ReadyQueue;

// ReadyQueue functions
void queue_init(ReadyQueue *q);
int queue_add(ReadyQueue *q, struct Ship ship);
int queue_remove(ReadyQueue *q, int index);
int queue_is_empty(const ReadyQueue *q);

//  Quantum for Round Robin
#define DEFAULT_QUANTUM 2

// Scheduler algorithms
int scheduler_fcfs(ReadyQueue *q);
int scheduler_sjf(ReadyQueue *q);
int scheduler_strn(ReadyQueue *q);
int scheduler_priority(ReadyQueue *q);
int scheduler_rr(ReadyQueue *q, int quantum, int *rr_index);
int scheduler_edf(ReadyQueue *q);

#endif // SCHEDULER_H
