#include "../common/plant.h"
#include "../common/err.h"
#include "headers/factory.h"
#include <stdio.h>
#include <assert.h>

static factory_t factory;
static bool factory_active = false;

// This is the function every worker thread will execute
void* worker_thread_func(void* arg) {
    worker_info_t* info = (worker_info_t*)arg;

    // TODO: Later you will implement the logic here (wait for tasks, etc.)
    // For now, we can just print or sleep to verify it works
    // worker_loop(wrapper); 
    
    return NULL;
}

void* manager_thread_func(void* arg)
{
    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));

    while (!atomic_load(&factory.is_terminated)) {
        bool task_started = false;

        for (size_t i = 0; i < factory.tasks.count; i++) {
            task_info_t* task = factory.tasks.items[i];
            
        }

        if (!task_started) {
            ASSERT_ZERO(pthread_cond_wait(&factory.manager_cond, &factory.main_lock));
        }
    }
    ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));
    return NULL;
}

int init_plant(int* stations, int n_stations, int n_workers)
{
    if (n_stations <= 0 || n_workers < 0 || stations == NULL || factory_active) {
        return -1;
    }

    ASSERT_ZERO(factory_init(&factory, n_stations, stations, n_workers));
    ASSERT_ZERO(pthread_create(&factory.manager_thread, NULL, manager_thread_func, &factory));
    factory_active = true;
    return 0;
}

int destroy_plant()
{
    if (!factory_active) {
        return -1;
    }

    atomic_store(&factory.is_terminated, true);

    pthread_mutex_lock(&factory.main_lock);
    pthread_cond_broadcast(&factory.manager_cond);
    pthread_mutex_unlock(&factory.main_lock);

    for (size_t i = 0; i < factory.workers.count; i++) {
        worker_info_t* w = factory.workers.items[i];
        pthread_mutex_lock(&w->worker_mutex);
        pthread_cond_signal(&w->wakeup_cond);
        pthread_mutex_unlock(&w->worker_mutex);
    }

    for (size_t i = 0; i < factory.workers.count; i++) {
        worker_info_t* w = factory.workers.items[i];
        pthread_join(w->thread_id, NULL);
    }

    pthread_join(factory.manager_thread, NULL);

    factory_destroy(&factory);
    factory_active = false;

    return 0;
}

int add_worker(worker_t* w)
{
    if (!factory_active || w == NULL) {
        return -1;
    }

    worker_info_t* wrapper = malloc(sizeof(worker_info_t));
    assert(wrapper != NULL);

    ASSERT_ZERO(worker_wrapper_init(wrapper, w, &factory));
    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));

    worker_cont_push_back(&factory.workers, wrapper);
    ASSERT_ZERO(pthread_create(&wrapper->thread_id, NULL, worker_thread_func, wrapper));

    ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));

    return 0;
}

int add_task(task_t* t)
{
    if (!factory_active || t == NULL) {
        return -1;
    }

    task_info_t* wrapper = malloc(sizeof(task_info_t));
    assert(wrapper != NULL);

    task_wrapper_init(wrapper, t);

    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));

    if (!factory_active || atomic_load(&factory.is_terminated)) {
        ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));
        task_wrapper_destroy(wrapper);
        free(wrapper);
        return -1;
    }

    task_cont_push_back(&factory.tasks, wrapper);

    ASSERT_ZERO(pthread_cond_signal(&factory.manager_cond));
    ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));

    return 0;
}

/* factory activate and so on should be inside mutex i belive */
int collect_task(task_t* t)
{
    if (!factory_active || t == NULL) {
        return -1;
    }

    task_info_t* wrapper = NULL;

    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));
    
    for (size_t i = 0; i < factory.tasks.count; i++) {
        if (factory.tasks.items[i]->original_def->id == t->id) {
            wrapper = factory.tasks.items[i];
            break;
        }
    }
    
    ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));

    if (wrapper == NULL) {
        return -1;
    }

    ASSERT_ZERO(pthread_mutex_lock(&wrapper->task_mutex));

    while (!wrapper->is_completed) {
        ASSERT_ZERO(pthread_cond_wait(&wrapper->task_complete_cond, &wrapper->task_mutex));
    }

    ASSERT_ZERO(pthread_mutex_unlock(&wrapper->task_mutex));
    return 0;
}