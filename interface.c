#include "interface.h"
#include "scheduler.h"

// Interface implementation
// Implement APIs here...

void init_scheduler(enum sch_type type, int thread_count)
{
    num_processes = thread_count;
    scheduler_type = type;
    // initialize queues
    PriorityQueue_init(&arrival_queue);
    PriorityQueue_init(&io_queue);
    ready_queue = malloc(sizeof(PriorityQueue) * MLFQ_LEVELS);
    for (int i = 0; i < MLFQ_LEVELS; i++) {
        PriorityQueue_init(ready_queue + i);
    }
    // init lock
    pthread_mutex_init(&execution_lock, NULL);

    //init semaphores
    semaphores = malloc(sizeof(Semaphore) * MAX_NUM_SEM);
    for(int i = 0; i < MAX_NUM_SEM; i++){
        semaphores[i].value = 0;
        PriorityQueue_init(&(semaphores[i].blocked));
    }


    // allocate and initialize PCBs for each thread / program
    program_control_blocks = malloc(sizeof(ProgramControlBlock) * thread_count);
    
    printf("Allocated Space for %d threads.\n", thread_count);

    // initialize PCBs
    for(int thread_id = 0; thread_id < thread_count; thread_id++) {
        ProgramControlBlock *currentPCB = program_control_blocks + thread_id;
        currentPCB->cpu_burst_length = 0;
        currentPCB->thread_id = thread_id;
        currentPCB->started = false;
        currentPCB->remaining_time_quanta = 0;
        currentPCB->arrival_time = 0;

        pthread_cond_init(&(currentPCB->allow_to_run), NULL);
    }

    printf("Finished PCB initialization\n");
}

int cpu_me(float current_time, int tid, int remaining_time)
{
    printf("cpu_me(%f, %d, %d)\n", current_time, tid, remaining_time);
    // puts the calling thread onto the ready queue

    if (remaining_time != 0){
        // pthread_mutex_lock(&execution_lock);
        int arrival_time = (int)ceil(current_time);
        start_check(arrival_time, tid);
        request_cpu(arrival_time, tid, remaining_time);
        schedule(tid);
        printf("%d - %d: T%d, CPU\n", wall_clock, wall_clock + 1, tid);
        tick(1);
        // pthread_mutex_unlock(&execution_lock);
    }
    return wall_clock;
}

int io_me(float current_time, int tid, int duration)
{
    printf("io_me(%f, %d, %d)\n", current_time, tid, duration);
    if (duration > 0) {
        printf("f\n");
        // pthread_mutex_lock(&execution_lock);
        printf("v\n");
        int arrival_time = (int)ceil(current_time);
        start_check(arrival_time, tid);
        request_io(arrival_time, tid, duration);
        schedule(tid);
        // pthread_mutex_unlock(&execution_lock);
    }

    return wall_clock;

}

int P(float current_time, int tid, int sem_id){   
    int arrival_time = (int)ceil(current_time);
    start_check(arrival_time, tid);
    printf("P --time:%f,tid:%d,semid:%d\n",current_time,tid,sem_id);
    request_P(arrival_time, tid, sem_id);
    return wall_clock;
}

int V(float current_time, int tid, int sem_id) //signal
{
    int arrival_time = (int)ceil(current_time);
    start_check(arrival_time, tid);

    printf("V--time:%f,tid:%d,semid:%d\n",current_time,tid,sem_id); 
    request_V(arrival_time, tid, sem_id);
    return wall_clock;    
}

void end_me(int tid)
{   
    num_running_processes --;
    if (num_running_processes > 0){
        // only schedule if there is more running processes
        schedule(tid);
    } else {
        // signal all other processes
        for (int thread_id = 0; thread_id < num_processes; thread_id++){
            pthread_cond_broadcast(&(program_control_blocks[thread_id].allow_to_run));
        }
    }
    num_started_processes --;
    bool is_last = num_started_processes == 0; // this check needs to be inside the critical section
    pthread_mutex_unlock(&execution_lock); // release the execution lock
    if (is_last) {
        // we are the last process to exit
        // clean up
        clean_up();
        printf("Exit memory clean up from thread %d\n", tid);
    }
}