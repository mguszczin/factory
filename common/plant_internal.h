#include <pthread.h>
#include <stdbool.h>
#include "plant.h"

// --- STAN WORKERA (Widziany przez Managera) ---
typedef struct {
    worker_t* original_def;   // Wskaźnik do oryginalnej struktury
    pthread_t thread_id;      // ID wątku systemowego
    
    // Synchronizacja dla konkretnego workera
    pthread_cond_t wakeup_cond; // Manager kopie workera w kostkę tym sygnałem
    
    // Co worker ma  robić?
    task_t* assigned_task;      // Zadanie do wykonania (NULL jeśli wolny)
    int assigned_result_index;  // Indeks 'i' w tablicy wyników
    bool terminate;             // Flaga: "Idź do domu (destroy_plant)"
} WorkerInfo;

// --- STAN ZADANIA (Wrapper) ---
typedef struct {
    task_t* original_def;
    int workers_assigned;     // Ilu pracowników JUŻ przydzielono (0..capacity)
    int workers_finished;     // Ilu pracowników JUŻ skończyło
    
    // Synchronizacja dla collect_task
    pthread_mutex_t task_mutex; 
    pthread_cond_t task_complete_cond; // collect_task na tym czeka
    bool is_completed;        // Czy wszyscy skończyli?
} TaskInfo;

// --- GŁÓWNA STRUKTURA FABRYKI ---
typedef struct {
    // Zasoby
    int* station_capacities;  // Kopia tablicy s_i
    int* station_usage;       // Ile miejsc jest aktualnie zajętych
    int n_stations;
    
    // Listy
    WorkerInfo** workers;     // Tablica wskaźników na naszych workerów
    int n_workers_registered; // Ilu dodało się przez add_worker
    int max_workers;          // P (z init_plant)
    
    TaskInfo** tasks;         // Lista/Tablica zadań (może być dynamiczna lista linkowana)
    int n_tasks;

    // --- SYNCHRONIZACJA CENTRALNA ---
    pthread_mutex_t main_lock;      // JEDEN MUTEX, BY RZĄDZIĆ WSZYSTKIMI
    pthread_cond_t manager_cond;    // Budzik dla managera (nowe zadanie, worker skończył)
    
    pthread_t manager_thread;       // Wątek prezesa
    bool plant_running;             // Czy fabryka działa?
} Plant;

// Globalna instancja (bo w C zadania są zazwyczaj globalne)
static Plant g_plant;