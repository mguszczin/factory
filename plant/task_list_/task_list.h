#ifndef TASK_LIST_H
#define TASK_LIST_H

#include <stdlib.h>
#include "plant.h"

typedef struct {
    task_t** items;
    size_t capacity;
    size_t count;
    size_t unfinished_tasks;
} task_container;

void task_list_init(task_container* list);
int task_list_push_back(task_container* list, task_t* task);
task_t* task_list_get(task_container* list, size_t index);
void task_list_free(task_container* list);

#endif