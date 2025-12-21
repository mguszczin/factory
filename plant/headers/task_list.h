#ifndef TASK_LIST_H
#define TASK_LIST_H

#include <stdlib.h>
#include "task_info.h"

typedef struct {
    task_info_t** items;
    size_t capacity;
    size_t count;

    size_t unfinished_tasks;
} task_container;

int task_cont_init(task_container* cont);
int task_cont_push_back(task_container* cont, task_info_t* task);
task_info_t* task_cont_get(task_container* cont, size_t index);
size_t task_cont_size(task_container* cont);
void task_cont_free(task_container* cont);

#endif