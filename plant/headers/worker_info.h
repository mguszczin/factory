#ifndef WORKER_INFO_H
#define WORKER_INFO_H

#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "../../common/plant.h"
#include "task_info.h"

struct factory_t;

typedef struct {
    worker_t* original_def;
    pthread_t thread_id;
    
    pthread_mutex_t worker_mutex;
    pthread_cond_t wakeup_cond;
    
    task_info_t* assigned_task;
    struct factory_t* my_factory;
} worker_info_t;

void worker_info_init(worker_info_t* info, worker_t* worker_def, struct factory_t* factory_t);
void worker_info_destroy(worker_info_t* info);

#endif