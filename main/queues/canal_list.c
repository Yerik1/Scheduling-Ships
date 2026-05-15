//
// Created by user on 4/27/2026.
//

#include "canal_list.h"

void canal_list_init(CanalList *cl) {
    if (cl == NULL){
        return;
    }

    cl->count = 0;

    for (int i = 0; i < CANAL_LEN; i++){
        cl->tasks[i] = NULL;
    }
}
int canal_list_add(CanalList *cl, ShipTask *task) {
    if (cl == NULL || task == NULL){
        return 0;
    }
    if (cl->count >= CANAL_LEN){
        return 0;
    }

    int entryIndex;
    if (task->ship.origin == LEFT) {
        entryIndex = 0;
    } else {
        entryIndex = CANAL_LEN - 1;
    }

    //Verifica que no haya barcos donde va a entrar
    if (cl->tasks[entryIndex] != NULL) {
        return 0;
    }

    cl->tasks[entryIndex] = task;
    task->ship.position = entryIndex;

    cl->count++;
    return 1;
}
ShipTask *canal_list_remove(CanalList *cl, int index) {
    if (cl == NULL) {
        return NULL;
    }

    if (index < 0 || index >= CANAL_LEN) {
        return NULL;
    }

    ShipTask *removedTask = cl->tasks[index];

    if (removedTask == NULL) {
        return NULL;
    }

    cl->tasks[index] = NULL;
    cl->count--;

    return removedTask;
}
ShipTask *canal_list_get(CanalList *cl, int index) {
    if (cl == NULL){
        return NULL;
    }

    if (index < 0 || index >= CANAL_LEN){
        return NULL;
    }

    return cl->tasks[index];
}
bool canal_list_empty(CanalList *cl) {
    if (cl == NULL){
        return true;
    }
    return cl->count == 0;
}

bool move_task(CanalList *cl, int oldIndex, int newIndex) {
    if (cl == NULL){
        return false;
    }
    if (oldIndex < 0 || oldIndex >= CANAL_LEN) {
        return false;
    }
    if (newIndex < 0 || newIndex >= CANAL_LEN) {
        return false;
    }

    if (cl->tasks[oldIndex] != NULL) {
        if (cl->tasks[newIndex] == NULL) {
            cl->tasks[newIndex] = cl->tasks[oldIndex];
            cl->tasks[oldIndex] = NULL;
            cl->tasks[newIndex]->ship.position = newIndex;
            return true;
        }
    }

    return false;
}

bool is_pos_free(CanalList *cl, int index) {
    if (cl == NULL || index < 0 || index >= CANAL_LEN) {
        return false;
    }
    if (cl->tasks[index] == NULL) {
        return true;
    }
    return false;
}