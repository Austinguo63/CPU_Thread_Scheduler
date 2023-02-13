#include "scheduler.h"
#include "assert.h"

// Scheduler implementation
// Implement all other functions here...

int num_processes;
int num_started_processes;
int num_running_processes;
ProgramControlBlock *program_control_blocks;
int wall_clock;
PriorityQueue arrival_queue;
PriorityQueue* ready_queue;
pthread_mutex_t execution_lock;
PriorityQueue io_queue;
Semaphore* semaphores;
enum sch_type scheduler_type;
int remaining_time_quanta;
ProgramControlBlock *current_cpu_program;

//int semaphores;

// priority queue operations
void PriorityQueue_init(PriorityQueue *queue){
    queue->head = NULL;
    queue->tail = NULL;
}

void PriorityQueue_enqueue(PriorityQueue *queue, ProgramControlBlock *block, int priority){
    assert(block != NULL);

    // create new entry
    PriorityQueueEntry *newEntry = malloc(sizeof(PriorityQueueEntry));
    newEntry->block = block;

    PriorityQueueEntry *current = NULL;
    PriorityQueueEntry *next = queue->head;

    // scan for the correct location to insert entry
    while(next != NULL && (next->priority < priority || ((next -> priority == priority) && (next->block->thread_id < block->thread_id)))){
        current = next;
        next = next->next;
    }
    
    newEntry->next = next;
    newEntry->priority = priority;

    // insert new entry
    if (next == NULL){
        // entry is the last entry
        queue->tail = newEntry;
    }

    if (current == NULL){
        // entry is the first entry
        queue->head = newEntry;
    } else {
        // entry is not the first entry
        current->next = newEntry;
    }
}

bool PriorityQueue_is_empty(PriorityQueue *queue){
    return queue->head == NULL;
}

ProgramControlBlock *PriorityQueue_dequeue(PriorityQueue *queue){
    assert(!PriorityQueue_is_empty(queue));
    
    // remove the first entry from the queue
    PriorityQueueEntry *head = queue->head;
    queue->head = head->next;

    if(queue->head == NULL) {
        queue->tail = NULL;
    }

    ProgramControlBlock *block = head->block;

    // deallocate the removed entry
    free(head);

    return block;
}

ProgramControlBlock *PriorityQueue_peek(PriorityQueue *queue){
    assert(!PriorityQueue_is_empty(queue));

    return queue->head->block;
}

void PriorityQueue_destroy(PriorityQueue *queue){
    // note it does not delete the queue stuct it self
    // just the nodes.
    while(!PriorityQueue_is_empty(queue)){
        PriorityQueue_dequeue(queue);
    }
}

void request_cpu(int arrival_time, int thread_id, int remaining_time){

    // printf("Req CPU (%d, %d, %d)\n", arrival_time, thread_id, remaining_time);

    ProgramControlBlock *block = &(program_control_blocks[thread_id]);
    if(block->cpu_burst_length == 0){
        block -> arrival_time = arrival_time;
    }

    block -> remaining_time = remaining_time;

    assert(arrival_time <= wall_clock);
    if (block->remaining_time_quanta == 0){
        add_to_ready_queue(block);
    }

    // block -> cpu_burst_length ++;
}

void request_io(int arrival_time, int thread_id, int duration){
    ProgramControlBlock *block = &(program_control_blocks[thread_id]);
    block->arrival_time = arrival_time;
    block->remaining_time = duration;
    block->cpu_burst_length = 0; // reset this counter since process is no using on cpu

    assert(arrival_time <= wall_clock);
    PriorityQueue_enqueue(&io_queue, block, wall_clock);
}


void request_P(int arrival_time, int thread_id, int sem_id){
    Semaphore *sem = semaphores + sem_id;
    ProgramControlBlock *block = &(program_control_blocks[thread_id]);
    block->cpu_burst_length = 0;
    sem->value --;
    if (sem->value < 0) {
        block ->arrival_time = arrival_time;
        block -> cpu_burst_length = 0; // since this will wait cpu.
        PriorityQueue_enqueue(&sem->blocked, block, arrival_time);
        schedule(thread_id);
    }

    assert(arrival_time <= wall_clock);
}

void request_V(int arrival_time, int thread_id, int sem_id){
    Semaphore *sem = semaphores + sem_id;
    ProgramControlBlock *block = &(program_control_blocks[thread_id]);
    block->cpu_burst_length = 0;
    sem->value ++;

    if (!PriorityQueue_is_empty(&sem->blocked)){
        ProgramControlBlock *released_block = PriorityQueue_dequeue(&sem->blocked);
        released_block->arrival_time = arrival_time;
        PriorityQueue_enqueue(&arrival_queue, released_block, arrival_time);
    }

    assert (arrival_time <= wall_clock);

}

void start_check(int arrival_time, int thread_id){
    ProgramControlBlock *block = &(program_control_blocks[thread_id]);
    if (! block->started) {
        // acquire lock
        pthread_mutex_lock(&execution_lock);
        block->started = true;
        num_running_processes ++;
        num_started_processes ++;
        printf("Started process id %d, %d currently started\n", thread_id, num_started_processes);

        block->arrival_time = arrival_time;

        // put the thread onto the arrival_queue
        PriorityQueue_enqueue(&arrival_queue, block, arrival_time);

        if(num_started_processes < num_processes) {
            // do not start scheduling operations until all threads have started
            pthread_cond_wait(&(block->allow_to_run), &execution_lock);
            return;
        } else {
            // start scheduling
            schedule(thread_id);
        }
    }
}

void schedule(int thread_id){
    ProgramControlBlock *block = &(program_control_blocks[thread_id]);
    
    // functionality moved to startup_check.
    // if (!block->started) {
    //     // current process has not started yet!
    //     block->started = true;
    //     num_running_processes ++;
    //     num_started_processes ++;
    //     printf("Started process id %d, %d currently started\n", thread_id, num_started_processes);
    //     if(num_started_processes < num_processes) {
    //         // do not start scheduling operations until all threads have started
    //         pthread_cond_wait(&(block->allow_to_run), &execution_lock);
    //         return;
    //     }
    // }

    // move members of future queue that have arrived at the current time step
    // move_from_future_queue();

    // TODO: remove debug prints
    printf("Schedule at tick %d, from thread %d\n", wall_clock, thread_id);
    print_future_queue();
    print_ready_queue();
    print_io_queue();


    // figure our who to execute next
    ProgramControlBlock *next_program_block = NULL;

    if (next_arrival() == 0) {
        next_program_block = pick_from_arrival_queue();
    }


    if (next_program_block == NULL && next_io_queue_completion() == 0){
        next_program_block = pick_from_io_queue();
    }

    if (next_program_block == NULL) {
        if (current_cpu_program != NULL && current_cpu_program->remaining_time_quanta != 0){
            next_program_block = current_cpu_program;
        } else {
            next_program_block = pick_from_ready_queue();
            current_cpu_program = next_program_block;
        }
    }

    if (next_program_block == NULL) {
        // at this point there is nothing to execute so we need to either wait for an io to finish
        // or an cpu job in the future queue to arrive.
        int next_arrival_event = next_arrival();
        int next_io_event = next_io_queue_completion();
        int next_event;

        if (next_io_event < 0 && next_arrival_event >= 0) {
            tick(next_arrival_event);
            next_program_block = pick_from_arrival_queue();
        } else if (next_arrival_event <0 && next_io_event >= 0) {
            tick(next_io_event);
            next_program_block = pick_from_io_queue();
        } else if (next_io_event >= 0 && next_arrival_event >= 0) {
            if(next_arrival_event > next_io_event) {
                tick(next_io_event);
                next_program_block = pick_from_io_queue();
            } else {
                tick(next_arrival_event);
                next_program_block = pick_from_arrival_queue();
            }
        }
    }

    assert(next_program_block != NULL);

    printf("Next thread is %d\n", next_program_block->thread_id);

    // run the chosen thread
    // since we signaled first and then wait, if the next thread is the same as the current thread
    // we'll enter dead lock
    // thus there is is check to skip the cond business if the current and next threads are the same.
    if (next_program_block->thread_id != thread_id) {
        // need the change threads
        pthread_cond_signal(&(next_program_block->allow_to_run)); // signal the thread we want to run
        pthread_cond_wait(&(block->allow_to_run), &execution_lock); // block execution of this thread
    }
    
    // continue with same thread again
}

ProgramControlBlock *pick_from_ready_queue(){
    // picks the next thread from the ready queue
    // removes the pick thread from the ready queue.
    // dependent on scheduler type.
    ProgramControlBlock *block = NULL;
    switch (scheduler_type)
    {
    case SCH_FCFS:
    case SCH_SRTF:{
        if (PriorityQueue_is_empty(ready_queue)) {
        return NULL;
        }
        block = PriorityQueue_dequeue(ready_queue);
        block->remaining_time_quanta = 1;
        break;
    }
    case SCH_MLFQ: {
        for (int i = 0; i < MLFQ_LEVELS; i++){
            if(! PriorityQueue_is_empty(ready_queue + i)){
                block = PriorityQueue_dequeue(ready_queue + i);
                block->remaining_time_quanta = MLFQ_TIME_QUANTUM[i];
                break;
            }
        }
        if (block == NULL){
            return NULL;
        }
        break;
    }
    }
    
    if (block -> remaining_time_quanta > block->remaining_time){
        block -> remaining_time_quanta = block->remaining_time;
    }

    return block;
}

ProgramControlBlock *pick_from_arrival_queue(){
    if (PriorityQueue_is_empty(&arrival_queue)) {
        return NULL;
    }

    return PriorityQueue_dequeue(&arrival_queue);
}

ProgramControlBlock *pick_from_io_queue(){
    if (PriorityQueue_is_empty(&io_queue)) {
        // no process in IO
        return NULL;
    }

    // ProgramControlBlock *current_IO = PriorityQueue_peek(&io_queue);
    // if (current_IO->remaining_time > 0) {
    //     // IO not completed yet
    //     return NULL;
    // }

    return PriorityQueue_dequeue(&io_queue);
}

void add_to_ready_queue(ProgramControlBlock *block){
    switch (scheduler_type)
    {
    case SCH_FCFS:{
        PriorityQueue_enqueue(ready_queue, block, block->arrival_time);
        break;
    }
    case SCH_SRTF:{
        PriorityQueue_enqueue(ready_queue, block, block->remaining_time);
        break;
    }
    case SCH_MLFQ: {
        if (block->cpu_burst_length == 0) {
            //first cpu request, always highest priority
            block->priority_level = 0;
        } else {
            //increment (lower) priority level since it is requesting cpu again
            if(block->priority_level < MLFQ_LEVELS){
                block->priority_level ++;
            }
        }
        PriorityQueue_enqueue(ready_queue + block->priority_level, block, wall_clock);
        break;
    }
    }
    
}

// void move_from_future_queue(){
//     if (PriorityQueue_is_empty(&arrival_queue)) {
//         return;
//     }

//     while (!PriorityQueue_is_empty(&arrival_queue) && PriorityQueue_peek(&arrival_queue)->arrival_time <= wall_clock) {
//         add_to_ready_queue(PriorityQueue_dequeue(&arrival_queue));
//     }
// }

// void move_from_future_io_queue(){
//     if (PriorityQueue_is_empty(&future_io_queue)) {
//         return;
//     }

//     while (!PriorityQueue_is_empty(&future_io_queue) && PriorityQueue_peek(&future_io_queue)->arrival_time <= wall_clock) {
//         Queue_enqueue(&io_queue, PriorityQueue_dequeue(&future_io_queue));
//     }
// }

void tick(int num_ticks){
    // advances the wall clock by this many ticks
    // use with caution, do not want to advance wall clock too far
    // includes sanity checks to attempt to prevent problems.
    wall_clock += num_ticks;

    if (!PriorityQueue_is_empty(&io_queue)){
        // decrement IO operation remaining time
        ProgramControlBlock *current_IO = PriorityQueue_peek(&io_queue);
        current_IO -> remaining_time -= num_ticks;
        
        // sanity check, time not ticked too far
        assert(current_IO -> remaining_time >= 0);
    }

    if (current_cpu_program != NULL){
        // decrement remaining cpu time quanta
        current_cpu_program->remaining_time_quanta -= num_ticks;
        // increment bust length counter.
        current_cpu_program->cpu_burst_length += num_ticks;
        // sanity check, time not ticked too far.
        assert(current_cpu_program -> remaining_time_quanta >= 0);
    }
}


/*int P_block(){
    if (PriorityQueue_is_empty(&P_queue)){
        return -1;
    }
    else {
        return PriorityQueue_peek(&)
    }

}
*/

int next_arrival(){
    if (PriorityQueue_is_empty(&arrival_queue)){
        return -1;
    } else {
        return PriorityQueue_peek(&arrival_queue)->arrival_time - wall_clock;
    }
}

int next_io_queue_completion(){
     if (PriorityQueue_is_empty(&io_queue)){
        return -1;
    } else {
        return PriorityQueue_peek(&io_queue)->remaining_time;
    }
}

void clean_up(){
    // deallocates and cleans up memory
    // destroy queues 
    PriorityQueue_destroy(&arrival_queue);
    PriorityQueue_destroy(&io_queue);
    
    for (int i = 0; i < MLFQ_LEVELS; i++) {
        PriorityQueue_destroy(ready_queue + i);
    }
    free(ready_queue); 
    // destroy lock
    pthread_mutex_destroy(&execution_lock);

    // destroy semaphores
    for(int i = 0; i < MAX_NUM_SEM; i++){
        PriorityQueue_destroy(&(semaphores[i].blocked));
    }
    free(semaphores);

    // destroy PCBs
    for(int thread_id = 0; thread_id < num_processes; thread_id++) {
        ProgramControlBlock *currentPCB = program_control_blocks + thread_id;
        pthread_cond_destroy(&(currentPCB->allow_to_run));
    }
    free(program_control_blocks);
}

void print_ready_queue(){
    PriorityQueueEntry *entry = ready_queue->head;
    printf("Ready Queue at %d:", wall_clock);
    while(entry != NULL) {
        printf("(%d, %d, %d),", entry->block->thread_id, entry->priority, entry->block->arrival_time);
        entry = entry->next;
    }
    printf("\n");
}

void print_future_queue(){
    PriorityQueueEntry *entry = arrival_queue.head;
    printf("futureQueue at %d:", wall_clock);
    while(entry != NULL) {
        printf("(%d, %d),", entry->block->thread_id, entry->block->arrival_time);
        entry = entry->next;
    }
    printf("\n");
}

void print_io_queue(){
    PriorityQueueEntry *entry = io_queue.head;
    printf("IO Queue at %d:", wall_clock);
    while(entry != NULL) {
        printf("(%d, %d),", entry->block->thread_id, entry->block->remaining_time);
        entry = entry->next;
    }
    printf("\n");
}
