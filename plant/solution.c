#include "../common/plant.h"
#include "../common/err.h"
#include "headers/factory.h"
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#undef errno

static factory_t factory;
static pthread_mutex_t main_lock = PTHREAD_MUTEX_INITIALIZER;
static int manager_signal = 0;
static int other_signal = 0;
static bool manager_should_sleep = false;

void notify_manager()
{
    manager_should_sleep = false;
    ASSERT_ZERO(pthread_cond_signal(&factory.manager_cond));
}

static bool worker_cond(worker_info_t* info)
{
    bool still_in_work = time(NULL) < info->original_def->end;
    bool needed_at_work = 
            (factory.is_terminated && factory.tasks.waiting_ans > 0) ||
            !factory.is_terminated;

    return still_in_work && needed_at_work;
}

static void* worker_thread_func(void* arg)
{
    worker_info_t* info = (worker_info_t*)arg;
    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    notify_manager();

    struct timespec ts;
    ts.tv_sec = info->original_def->end + 1;
    ts.tv_nsec = 0;

    while (worker_cond(info)) {
        int ret;
        while (info->assigned_task == NULL && worker_cond(info)) {
            ret = pthread_cond_timedwait(&info->wakeup_cond, &main_lock, &ts);
            if (ret != 0 && ret != ETIMEDOUT) syserr("Someting went wrong inside worker_tread_cond");
        }

        if (info->assigned_task == NULL) {
            break;
        }

        task_info_t* task = info->assigned_task;
        int my_idx = info->assigned_index;
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

        int res = info->original_def->work(info->original_def, task->original_def, my_idx);

        ASSERT_ZERO(pthread_mutex_lock(&main_lock));
        
        task->original_def->results[my_idx] = res;
        task->workers_assigned--;
        factory.station_usage[task->assigned_position]--;

        if (task->workers_assigned == 0) {
            task->is_completed = true;
            ASSERT_ZERO(pthread_cond_broadcast(&task->task_complete_cond));
        }

        info->assigned_task = NULL;
        info->assigned_index = -1;
        notify_manager();
    }

    /* if we leave we want to signal to the manager that we left and we might be able to terminate, because not enought workers*/
    notify_manager();

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
    return NULL;
}

/* Marks the task as failed and tells the listenting thread about it if there is one.*/
static void fail_task(task_info_t* task)
{
    task->is_completed = true;
    task->failed = true;
    ASSERT_ZERO(pthread_cond_broadcast(&task->task_complete_cond));
}

/* Find the smallest free station that is big enough*/
static int get_station_index(task_info_t* task)
{
    int workers_needed = task->original_def->capacity;
    int best_index = -1;
    int min_suitable_capacity = INT_MAX;
    bool is_size_present = false;

    for (size_t i = 0; i < factory.n_stations; i++) {
        int cap = factory.station_capacity[i];
        
        if (cap >= workers_needed) {
            is_size_present = true;

            if (factory.station_usage[i] == 0 && cap <= min_suitable_capacity) {
                min_suitable_capacity = cap;
                best_index = (int)i;
            }
        }
    }

    if (!is_size_present)
        fail_task(task);

    return best_index;
}

static bool free_workers_present(task_info_t* task, const time_t now)
{
    int workers_needed = task->original_def->capacity;
    int available = 0;
    int bad_workers = 0;

    /* Check for avaiable workers */
    for (size_t i = 0; i < factory.workers.count; i++) {
        worker_info_t* w = factory.workers.items[i];
        time_t best = now;
        if (best < task->original_def->start)
            best = task->original_def->start;
        
        if (w->assigned_task == NULL && 
            best >= w->original_def->start && 
            best < w->original_def->end) {
            available++;
        }

        if (available >= workers_needed) 
            return true;

        if (best >= w->original_def->end)
            bad_workers++;
    }

    int potential_worker_size = factory.is_terminated ? 
                                factory.workers.count : 
                                factory.workers.capacity;
    if ((potential_worker_size - bad_workers) < workers_needed)
        fail_task(task);
    
    return false;
}

static void assign_workers(const int best_ind, task_info_t* task, const time_t now)
{   
    int workers_needed = task->original_def->capacity;

    factory.station_usage[best_ind] = workers_needed;
    task->workers_assigned = workers_needed;
    task->assigned_position = best_ind;

    int current_worker_index = 0;
    for (size_t i = 0; i < factory.workers.count; i++) {
        worker_info_t* w = factory.workers.items[i];

        if (w->assigned_task == NULL && now >= w->original_def->start && now < w->original_def->end) {
            w->assigned_task = task;
            w->assigned_index = current_worker_index;
            ASSERT_ZERO(pthread_cond_signal(&w->wakeup_cond));

            current_worker_index++;
            if (workers_needed == current_worker_index) return;
        }
    }
    syserr("Something went wrong inside assign workers, the count isn't probably well done");
}

/* Ta funkcja wydaje sie być raczej dabliu */
static void* manager_thread_func(void* arg)
{
    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    while (!factory.is_terminated || (factory.is_terminated && factory.tasks.waiting_ans > 0)) {
        time_t now;
        time_t next_wakeup = 0;
        time_t starting_time = time(NULL);
        if (manager_should_sleep == true) manager_signal++;
        else other_signal++;

        //printf("MANAGER SIGNAL: %d\n", manager_signal);
        //printf("FACTORY SIGNAL: %d\n", other_signal);
        //printf("up : %d | down :  %d \n", up, down);
        for (size_t i = 0; i < factory.tasks.count; i++) {
            task_info_t* task = factory.tasks.items[i];
            /* `now` update is expensive but we want to maximize the 
               possiblity of assigning some free worker, because with `now` 
               updating here we have smaller time windows but the throughput
               is bigger. */
            now = time(NULL);

            if (task->is_completed || task->workers_assigned > 0) continue;

            if (task->original_def->start > now) {
                if (next_wakeup == 0 || task->original_def->start < next_wakeup) {
                    next_wakeup = task->original_def->start;
                }
            }

            int best_ind;
            if (free_workers_present(task, now) && (best_ind = get_station_index(task)) != -1 && task->original_def->start <= now) {
                assign_workers(best_ind, task, now);
            }
        }

        /* Set next wakup for worker */
        for (int i = 0; i < factory.workers.count; i++) {
            worker_info_t* worker = factory.workers.items[i];
            if (worker->original_def->start > now) {
                if (next_wakeup == 0 || worker->original_def->start < next_wakeup) {
                    next_wakeup = worker->original_def->start;
                }
            }
        }
        int res = 0;
        manager_should_sleep = true;
        if (next_wakeup > 0 && next_wakeup > starting_time) {
            //printf("next_wakup: %ld | starting time: %ld\n", next_wakeup, starting_time);
            struct timespec ts;
            // for some reason this avoids a lot of spinning
            ts.tv_sec = next_wakeup + 1;
            ts.tv_nsec = 0;
            while((res == 0 && manager_should_sleep == true)) {
                res = pthread_cond_timedwait(&factory.manager_cond, &main_lock, &ts);
                if (res != 0 && res != ETIMEDOUT) syserr("pthread condition unexpected finish");
            }
        } else {
            while(manager_should_sleep) {
                ASSERT_ZERO(pthread_cond_wait(&factory.manager_cond, &main_lock));
            }
        }
    }

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
    return NULL;
}

/* Function initializes factory if it hasn't been initialized before.
   Does memory allocation before entering mutex for efficiency */
int init_plant(int* stations, int n_stations, int n_workers)
{
    factory_t f = {0};
    if (factory_init(&f, n_stations, stations, n_workers) != 0)
        return ERROR;
    
    /* Now we enter mutex end check if we can still initialize factory */
    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    if (factory.is_active == true) {
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        factory_destroy(&f);
        return ERROR;
    }

    factory = f;

    /* We need to remember to destroy the condition when leaving mutex. */
    if (pthread_cond_init(&factory.manager_cond, NULL) != 0) {
        factory_destroy(&factory);
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        return ERROR;
    }

    if (pthread_create(&factory.manager_thread, NULL, manager_thread_func, NULL) != 0) {
        pthread_cond_destroy(&factory.manager_cond);
        factory_destroy(&factory);
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        return ERROR;
    }

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
    return PLANTOK;
}

/* We terminate the whole plantation and wait for it to handle all tasks,
   that run currently. */
int destroy_plant()
{
    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    if (factory.is_active == false || factory.is_terminated == true) {
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        return ERROR;
    }

    factory.is_terminated = true;
    notify_manager();

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

    ASSERT_ZERO(pthread_join(factory.manager_thread, NULL));
    /* After manager left we signall all remaining 
       workers so they can leave */
    ASSERT_ZERO(pthread_mutex_lock(&main_lock));
    int size = worker_cont_size(&factory.workers);
    for (size_t i = 0; i < size; i++) {
        worker_info_t* w = factory.workers.items[i];
        ASSERT_ZERO(pthread_cond_signal(&w->wakeup_cond));
    }
    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

    for (size_t i = 0; i < factory.workers.count; i++) {
        worker_info_t* w = factory.workers.items[i];
        ASSERT_ZERO(pthread_join(w->thread_id, NULL));
    }


    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    factory_destroy(&factory);
    ASSERT_ZERO(pthread_cond_destroy(&factory.manager_cond));

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

    return PLANTOK;
}

int add_worker(worker_t* w)
{
    if (!w) {
        return ERROR;
    }

    worker_info_t* wrapper = calloc(1,sizeof(worker_info_t));
    if (!wrapper) return ERROR;

    if (worker_info_init(wrapper, w) != 0) {
        free(wrapper);
        return ERROR;
    }

    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    if (factory.is_terminated || !factory.is_active) {
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        worker_info_destroy(wrapper);
        free(wrapper);
        return ERROR;
    }

    int prev_size = factory.workers.count;
    worker_cont_push_back(&factory.workers, wrapper);
    int cur_size = factory.workers.count;

    if(prev_size != cur_size && 
        pthread_create(&wrapper->thread_id, NULL, worker_thread_func, wrapper) != 0) {
        worker_info_destroy(wrapper);
        free(wrapper);
        factory.workers.count--;
        return ERROR;
    }

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

    return PLANTOK;
}

int add_task(task_t* t)
{
    if (!t) {
        return ERROR;
    }

    task_info_t* wrapper = calloc(1, sizeof(task_info_t));
    if(!wrapper) return ERROR;

    if (task_info_init(wrapper, t) != 0) {
        free(wrapper);
        return ERROR;
    }

    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    int prev_size = factory.tasks.count;
    int ret = task_cont_push_back(&factory.tasks, wrapper);
    int cur_size = factory.tasks.count;

    bool is_not_ready = factory.is_terminated || !factory.is_active;
    if (is_not_ready || ret != 0) {
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        task_info_destroy(wrapper);
        free(wrapper);
        return ERROR;
    }

    /* If we really added a new element tell manager about the update. */
    if (prev_size != cur_size) {
        notify_manager();
    }

    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

    return PLANTOK;
}

static bool can_be_collected(task_t *t, task_info_t** wrapper)
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
    if (!t)
        return ERROR;
    
    task_info_t* wrapper = NULL;

    ASSERT_ZERO(pthread_mutex_lock(&main_lock));

    if (factory.is_terminated || !factory.is_active || 
                            !can_be_collected(t, &wrapper)) {
        ASSERT_ZERO(pthread_mutex_unlock(&main_lock));
        return ERROR;
    }
    
    factory.tasks.waiting_ans++;
    while (!wrapper->is_completed) {
        ASSERT_ZERO(pthread_cond_wait(&wrapper->task_complete_cond, &main_lock));
    }
    factory.tasks.waiting_ans--;
    /* We need to read here because destroy may be called. */
    bool bad = wrapper->failed;

    if (factory.tasks.waiting_ans == 0 && factory.is_terminated) {
        notify_manager();
    }
    ASSERT_ZERO(pthread_mutex_unlock(&main_lock));

    if (bad) return ERROR;
    return PLANTOK;
}