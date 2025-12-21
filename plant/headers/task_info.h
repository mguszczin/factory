#ifndef TASK_INFO_H
#define TASK_INFO_H

#include <pthread.h>
#include <stdbool.h>
#include "../../common/plant.h"

typedef struct {
    task_t* original_def;
    int workers_assigned;
    int workers_finished;
    int assigned_position;
    
    pthread_mutex_t task_mutex; 
    pthread_cond_t task_complete_cond;
    bool is_completed;
} task_info_t;

void task_info_init(task_info_t* info, task_t* task_def);
void task_info_destroy(task_info_t* info);

#endif