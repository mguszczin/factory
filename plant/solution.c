#include "../common/plant.h"
#include "../common/err.h"
#include "headers/factory.h"
#include <stdio.h>
#include <assert.h>
#include <time.h>

static factory_t factory;
static pthread_mutex_t main_lock = PTHREAD_MUTEX_INITIALIZER;

void* worker_thread_func(void* arg) {
    worker_info_t* info = (worker_info_t*)arg;
    
    /* Notify manager that i am free. */
    ASSERT_ZERO(pthread_mutex_lock(&factory.main_lock));
    ASSERT_ZERO(pthread_cond_signal(&factory.manager_cond));
    ASSERT_ZERO(pthread_mutex_unlock(&factory.main_lock));
    /* Consider finishing at end or what if we are terminated */
    while (time(NULL) < info->original_def->end) {
        ASSERT_ZERO(pthread_mutex_lock(&info->worker_mutex));
        while (info->assigned_task == NULL) {
            ASSERT_ZERO(pthread_cond_wait(&info->wakeup_cond, &info->worker_mutex));
        }

        task_info_t* task = info->assigned_task;
        int my_idx = info->assigned_index;
        ASSERT_ZERO(pthread_mutex_unlock(&info->worker_mutex));

        int res = info->original_def->work(info->original_def, task->original_def, my_idx);
        task->original_def->results[my_idx] = res;

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

/* Function initializes factory if it hasn't been initialized before.
   Does memory allocation before entering mutex for efficiency */
int init_plant(int* stations, int n_stations, int n_workers)
{
    factory_t f;
    if (factory_init(&f, n_stations, stations, n_workers) != 0)
        return -1;
    
    /* Now we enter mutex end check if we can still initialize factory */
    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    if (factory.is_active == true) {
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        factory_destroy(&f);
        return -1;
    }

    factory = f;
    /* We need to remember to destroy the condition when leaving mutex. */
    ASSERT_ZERO(pthread_cond_init(&factory.manager_cond, NULL));

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
    return 0;
}

/* We destroy the whole plantation */
int destroy_plant()
{
    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    if (factory.is_active == false || factory.is_terminated == true) {
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        return -1;
    }

    factory.is_terminated = true;

    int size = worker_cont_size(&factory.workers);
    for (size_t i = 0; i < size; i++) {
        worker_info_t* w = factory.workers.items[i];
        ASSERT_ZERO(pthread_cond_signal(&w->wakeup_cond));
    }
    ASSERT_ZERO(pthread_cond_signal(&factory.manager_cond));

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

    for (size_t i = 0; i < factory.workers.count; i++) {
        worker_info_t* w = factory.workers.items[i];
        pthread_join(w->thread_id, NULL);
    }

    pthread_join(factory.manager_thread, NULL);


    ASSERT_ZERO(pthread_mutex_lock(&main_lock));
    factory_destroy(&factory);
    ASSERT_ZERO(pthread_cond_destroy(&factory.manager_cond));
    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

    return 0;
}

int add_worker(worker_t* w)
{
    if (!w) {
        return -1;
    }

    worker_info_t* wrapper = malloc(sizeof(worker_info_t));
    if (!wrapper) return -1;

    if (worker_info_init(wrapper, w, &factory) != 0) {
        free(wrapper);
        return -1;
    }

    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    if (factory.is_terminated || !factory.is_active) {
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        worker_info_destroy(wrapper);
        free(wrapper);
        return -1;
    }

    worker_cont_push_back(&factory.workers, wrapper);
    ASSERT_ZERO(pthread_create(&wrapper->thread_id, NULL, worker_thread_func, wrapper));

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

    return 0;
}

int add_task(task_t* t)
{
    if (!t) {
        return -1;
    }

    task_info_t* wrapper = malloc(sizeof(task_info_t));
    if(!wrapper) return -1;

    if (task_wrapper_init(wrapper, t) != 0) {
        free(wrapper);
        return -1;
    }

    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    if (factory.is_terminated || !factory.is_active) {
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        task_wrapper_destroy(wrapper);
        free(wrapper);
        return -1;
    }

    task_cont_push_back(&factory.tasks, wrapper);
    ASSERT_ZERO(pthread_cond_signal(&factory.manager_cond));

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

    return 0;
}

bool can_be_collected(task_t *t, task_info_t** wrapper)
{
    for (size_t i = 0; i < factory.tasks.count; i++) {
        task_info_t* cur_task = factory.tasks.items[i];

        if (cur_task->original_def->id == t->id) {
            *wrapper = cur_task;
            break;
        }
    }

    if (*wrapper == NULL) {
        return false;
    }
    return true;
}

int collect_task(task_t* t)
{
    if (!t) {
        return -1;
    }
    task_info_t* wrapper = NULL;

    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    if (factory.is_terminated || !factory.is_active || 
                            !can_be_collected(t, &wrapper)) {
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        return -1;
    }
    
    factory.tasks.waiting_ans++;
    while (!wrapper->is_completed) {
        ASSERT_ZERO(pthread_cond_wait(&wrapper->task_complete_cond, &main_lock));
    }
    factory.tasks.waiting_ans--;
    /* We need to read here because destroy may be called. */
    bool bad = wrapper->failed;

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

    if (bad) return -1;
    return 0;
}