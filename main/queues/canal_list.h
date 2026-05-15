//
// Created by user on 4/27/2026.
//

#ifndef CANAL_LIST_H
#define CANAL_LIST_H

#include "../tasks/ship_task.h"
#define CANAL_LEN 10


typedef struct {
    ShipTask *tasks[CANAL_LEN];
    int count;
} CanalList;

void canal_list_init(CanalList *cl);
int canal_list_add(CanalList *cl, ShipTask *task);
ShipTask *canal_list_remove(CanalList *cl, int index);
ShipTask *canal_list_get(CanalList *cl, int index);
bool canal_list_empty(CanalList *cl);
bool move_task(CanalList *cl, int oldIndex, int newIndex);
bool is_pos_free(CanalList *cl, int index);


#endif //CANAL_LIST_H
