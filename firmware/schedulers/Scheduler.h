#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stddef.h>
#include "../queues/ready_queue.h"

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
