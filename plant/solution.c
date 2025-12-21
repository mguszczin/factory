#include "../common/plant.h"
#include "../common/err.h"
#include "headers/factory.h"
#include <stdio.h>
#include <assert.h>
#include <time.h>

static factory_t factory;
static bool factory_active = false;

void* worker_thread_func(void* arg) {
    worker_info_t* info = (worker_info_t*)arg;
    
    /* Notify manager that i am free. */
    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));
    ASSERT_ZERO(pthread_cond_signal(&factory.manager_cond));
    ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));
    /* Consider finishing at end or what if we are terminated */
    while (true) {
        ASSERT_ZERO(pthread_mutex_lock(&info->worker_mutex));
        while (info->assigned_task == NULL) {
            ASSERT_ZERO(pthread_cond_wait(&info->wakeup_cond, &info->worker_mutex));
        }

        task_info_t* task = info->assigned_task;
        int my_idx = info->assigned_index;
        ASSERT_ZERO(pthread_mutex_unlock(&info->worker_mutex));

        /* Probably have to do something with res.*/
        int res = info->original_def->work(info->original_def, task->original_def, my_idx);

        ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));
        
        task->workers_assigned--;
        factory.station_usage[task->assigned_position]--;

        if (task->workers_assigned == 0) {
            ASSERT_ZERO(pthread_mutex_lock(&task->task_mutex));
            task->is_completed = true;
            ASSERT_ZERO(pthread_cond_broadcast(&task->task_complete_cond));
            ASSERT_ZERO(pthread_mutex_unlock(&task->task_mutex));
        }

        info->assigned_task = NULL;
        
        ASSERT_ZERO(pthread_cond_signal(&factory.manager_cond));
        ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));
    }

    return NULL;
}

void fail_task(task_info_t* task) {
    ASSERT_ZERO(pthread_mutex_lock(&task->task_mutex));
    task->is_completed = true;
    task->failed = true;
    ASSERT_ZERO(pthread_cond_broadcast(&task->task_complete_cond));
    ASSERT_ZERO(pthread_mutex_unlock(&task->task_mutex));
}

int get_station_index(task_info_t* task, time_t now)
{
    int required = task->original_def->capacity;
    int available = 0;
    int left = 0;
    int bad_workers = 0;

    /* Check for avaiable workers */
    for (size_t i = 0; i < factory.workers.count; i++) {
        worker_info_t* w = factory.workers.items[i];
        if (w->assigned_task == NULL && 
            now >= w->original_def->start && 
            now < w->original_def->end) {
            available++;
        }

        if (now >= w->original_def->end)
            bad_workers++;
    }

    int size = factory.is_terminated ? factory.workers.count : factory.workers.capacity;
    if ((size - bad_workers) < required) {
        fail_task(task);
        return -1;
    }

    if (available < required) {
        return -1;
    }

    /* Find the smallest station that is big enough*/
    int best_index = -1;
    int min_suitable_capacity = 2147483647;
    bool is_size_present = false;

    for (size_t i = 0; i < factory.n_stations; i++) {
        if (factory.station_usage[i] == 0 && factory.station_capacity[i] >= required) {
            if (factory.station_capacity[i] <= min_suitable_capacity) {
                min_suitable_capacity = factory.station_capacity[i];
                best_index = (int)i;
            }
        }
        if (factory.station_capacity[i] >= required) is_size_present = true;
    }

    if (!is_size_present) {
        fail_task(task);
        return -1;
    }

    return best_index;
}

void assign_workers(int best_ind, task_info_t* task, time_t now)
{   
    int workers_needed = task->original_def->capacity;
    int current_worker_index = 0;

    factory.station_usage[best_ind] = workers_needed;
    task->assigned_position = best_ind;
    task->workers_assigned = workers_needed;

    for (size_t i = 0; i < factory.workers.count; i++) {
        worker_info_t* w = factory.workers.items[i];
        if (w->assigned_task == NULL && now >= w->original_def->start && now < w->original_def->end) {
            ASSERT_ZERO(pthread_mutex_lock(&w->worker_mutex));
            w->assigned_task = task;
            w->assigned_index = current_worker_index;
            ASSERT_ZERO(pthread_cond_signal(&w->wakeup_cond));
            ASSERT_ZERO(pthread_mutex_unlock(&w->worker_mutex));

            current_worker_index++;
            if (workers_needed == current_worker_index) return;
        }
    }
    assert(0);
}

void* manager_thread_func(void* arg)
{
    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));

    while (!factory.is_terminated || (factory.is_terminated && factory.tasks.waiting_ans > 0)) {
        
        time_t now;
        time_t next_wakeup = 0;

        for (size_t i = 0; i < factory.tasks.count; i++) {
            task_info_t* task = factory.tasks.items[i];
            now = time(NULL);

            if (task->is_completed || task->workers_assigned > 0) continue;

            if (task->original_def->start > now) {
                if (next_wakeup == 0 || task->original_def->start < next_wakeup) {
                    next_wakeup = task->original_def->start;
                }
                continue;
            }

            int best_ind = get_station_index(task, now);
            if (best_ind != -1) {
                assign_workers(best_ind, task, now);
            }
        }

        if (next_wakeup > 0) {
            struct timespec ts;
            ts.tv_sec = next_wakeup;
            ts.tv_nsec = 0;
            pthread_cond_timedwait(&factory.manager_cond, &factory.main_lock, &ts);
        } else {
            ASSERT_ZERO(pthread_cond_wait(&factory.manager_cond, &factory.main_lock));
        }
    }

    ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));
    return NULL;
}

int init_plant(int* stations, int n_stations, int n_workers)
{
    if (!stations || factory_active) {
        return -1;
    }

    if (factory_init(&factory, n_stations, stations, n_workers) != 0) {
        return -1;
    }
    if (pthread_create(&factory.manager_thread, NULL, manager_thread_func, &factory) != 0) {
        factory_destroy(&factory);
        return -1;
    }
    factory_active = true;
    return 0;
}

int destroy_plant()
{
    if (!factory_active) {
        return -1;
    }

    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));

    factory.is_terminated = true;

    for (size_t i = 0; i < factory.workers.count; i++) {
        worker_info_t* w = factory.workers.items[i];
        ASSERT_ZERO(pthread_mutex_lock(&w->worker_mutex));
        ASSERT_ZERO(pthread_cond_signal(&w->wakeup_cond));
        ASSERT_ZERO(pthread_mutex_unlock(&w->worker_mutex));
    }
    ASSERT_ZERO(pthread_cond_broadcast(&factory.manager_cond));

    ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));

    for (size_t i = 0; i < factory.workers.count; i++) {
        worker_info_t* w = factory.workers.items[i];
        pthread_join(w->thread_id, NULL);
    }

    pthread_join(factory.manager_thread, NULL);

    factory_active = false;
    factory_destroy(&factory);

    return 0;
}

int add_worker(worker_t* w)
{
    if (!w || !factory_active) {
        return -1;
    }

    worker_info_t* wrapper = malloc(sizeof(worker_info_t));
    if (!wrapper) return -1;
    worker_info_init(wrapper, w, &factory);

    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));
    // factory shut down
    if (factory.is_terminated) {
        ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));
        worker_info_destroy(wrapper);
        free(wrapper);
        return -1;
    }

    worker_cont_push_back(&factory.workers, wrapper);
    ASSERT_ZERO(pthread_create(&wrapper->thread_id, NULL, worker_thread_func, wrapper));

    ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));

    return 0;
}

int add_task(task_t* t)
{
    if (!factory_active || !t) {
        return -1;
    }

    task_info_t* wrapper = malloc(sizeof(task_info_t));
    if(!wrapper) return -1;

    task_wrapper_init(wrapper, t);

    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));

    if (factory.is_terminated) {
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
    if (!factory_active || !t) {
        return -1;
    }

    task_info_t* wrapper = NULL;

    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));

    if (factory.is_terminated) {
        ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));
        return -1;
    }

    for (size_t i = 0; i < factory.tasks.count; i++) {
        if (factory.tasks.items[i]->original_def->id == t->id) {
            wrapper = factory.tasks.items[i];
            break;
        }
    }

    if (wrapper == NULL) {
        ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));
        return -1;
    }

    factory.tasks.waiting_ans++;
    
    ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));

    ASSERT_ZERO(pthread_mutex_lock(&wrapper->task_mutex));
    while (!wrapper->is_completed) {
        ASSERT_ZERO(pthread_cond_wait(&wrapper->task_complete_cond, &wrapper->task_mutex));
    }
    ASSERT_ZERO(pthread_mutex_unlock(&wrapper->task_mutex));

    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));
    factory.tasks.waiting_ans--;
    ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));

    if (wrapper->failed) return -1;
    return 0;
}