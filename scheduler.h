/**
 * @file
 * @author Jinyu Liu (jzl6359@psu.edu)
 * @author Hongyu Guo (hbg5147@psu.eud)
 * @brief Scheduler function and data structures.
 * @date 2022-10-10
 * 
 */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>

#include "interface.h"

// Declare your own data structures and functions here...

/**
 * @brief The number of levels to use in MLFQ
 * 
 * Calculated from the length of the array MLFQ_TIME_QUANTUM
 * 
 */
#define MLFQ_LEVELS sizeof(MLFQ_TIME_QUANTUM) / sizeof(int)

/**
 * @brief Holds information about each thread in the system
 * 
 */
struct ProgramControlBlockS{
    int thread_id;                  ///< Unique id of the thread
    int cpu_burst_length;           ///< How long has the current cpu burst lasted
    bool started;                   ///< Has the thread been started (specifically has it called startup_check() yet)
    int arrival_time;               ///< The #wall_clock time whe the thread requested the current activity
    int remaining_time;             ///< How much longer in ticks do the current activity require
    int remaining_time_quanta;      ///< How many ticks remain in the threads CPU time quanta allocation
    int priority_level;             ///< The priority level of the thread (only used in MLFQ)
    pthread_cond_t allow_to_run;    ///< A conditional used to block and unblock execution of this thread
};

/**
 * @brief Shorthand for struct ProgramControlBlockS
 * 
 */
typedef struct ProgramControlBlockS ProgramControlBlock;

// Priority Queue
// priority queue data structure and operations

/**
 * @brief An Entry in the Priority Queue
 * 
 */
struct PriorityQueueEntryS{
    int priority;                       ///< priority of the current entry
    struct PriorityQueueEntryS *next;   ///< pointer to data
    ProgramControlBlock *block;         ///< pointer to next entry, or Null if this is the last entry
};

/**
 * @brief Shorthand for struct PriorityQueueEntryS
 * 
 */
typedef struct PriorityQueueEntryS PriorityQueueEntry;

/**
 * @brief An Priority Queue for ProgramControlBlocks
 * 
 */
struct PriorityQueueS {
    PriorityQueueEntry *head;   ///< head of the queue
    PriorityQueueEntry *tail;   ///< tail of the queue
};

/**
 * @brief Shorthand for struct PriorityQueueS
 * 
 */
typedef struct PriorityQueueS PriorityQueue;

/**
 * @brief A Semaphore
 * 
 */
struct SemaphoreS {
    int value;              ///< the current value of the semaphore
    PriorityQueue blocked;  ///< a queue for holding threads blocks by this semaphore
};

/**
 * @brief Shorthand for struct SemaphoreS
 * 
 */
typedef struct SemaphoreS Semaphore;

/**
 * @brief Initialize the PriorityQueue
 * 
 * This should be called before any other operations are made to the queue
 * 
 * @param queue queue to operate on
 */
void PriorityQueue_init(PriorityQueue *queue);

/**
 * @brief Enqueue a ProgramControlBlock onto the queue according to priority
 * 
 * Ties are broken using the block's thread_id with lower thread ids getting higher priority.
 * 
 * @param queue the queue to operate on
 * @param block the block to enqueue
 * @param priority the priority to assign to this block
 */
void PriorityQueue_enqueue(PriorityQueue *queue, ProgramControlBlock *block, int priority);

/**
 * @brief Returns the first entry from the queue and removes it from the queue.
 * 
 * Do not call this function with an empty queue.
 * Use PriorityQueue_is_empty() first to make sure queue is not empty.
 * 
 * @param queue the queue to operate on
 * @return ProgramControlBlock* the first entry in the queue
 */
ProgramControlBlock *PriorityQueue_dequeue(PriorityQueue *queue);

/**
 * @brief Returns the first entry from the queue without removing it from the queue.
 * 
 * Do not call this function with an empty queue.
 * Use PriorityQueue_is_empty() first to make sure queue is not empty.
 * 
 * @param queue the queue to operate on
 * @return ProgramControlBlock* the first entry in the queue
 */
ProgramControlBlock *PriorityQueue_peek(PriorityQueue *queue);

/**
 * @brief Returns wether the queue is empty or not
 * 
 * This should be called to ensure the queue is not empty before calling
 * PriorityQueue_dequeue() or PriorityQueue_peek()
 * 
 * @param queue the queue to operate on
 * @return true the queue is empty
 * @return false the queue has at least one entry
 */
bool PriorityQueue_is_empty(PriorityQueue *queue);

/**
 * @brief Removes and deallocate all entries in the queue
 * 
 * Note this does not deallocate the blocks, just the PriorityQueueEntries
 * 
 * @param queue the queue to destroy
 */
void PriorityQueue_destroy(PriorityQueue *queue);
// scheduler functions

/**
 * @brief Semaphore post implementation
 * 
 * @param arrival_time The current wall_clock time
 * @param thread_id The id of the calling thread
 * @param sem_id The id of the semaphore to operate on
 */
void request_P(int arrival_time, int thread_id, int sem_id);

/**
 * @brief Semaphore wait implementation
 * 
 * @param arrival_time The current wall_clock time
 * @param thread_id The id of the calling thread
 * @param sem_id The id of the semaphore to operate on
 */
void request_V(int arrival_time, int thread_id, int sem_id);

/**
 * @brief Request to use the CPU
 * 
 * @param arrival_time wall_clock time of arrival
 * @param thread_id process requesting the CPU
 * @param remaining_time how many more consecutive ticks will this process need the cpu
 */
void request_cpu(int arrival_time, int thread_id, int remaining_time);

/**
 * @brief Request to use IO
 * 
 * @param arrival_time wall_clock time of arrival
 * @param thread_id process requesting the IO
 * @param duration how long will the IO operation take.
 */
void request_io(int arrival_time, int thread_id, int duration);

/**
 * @brief Performs startup synchronization
 * 
 * Does nothing once the thread has been started.
 * Call this before doing anything else in cpu_me(), io_me(), P(), V().
 * 
 * @param arrival_time current wall_time
 * @param thread_id id of the calling thread
 */
void start_check(int arrival_time, int thread_id);

/**
 * @brief Performs scheduling operations.
 * 
 * This program should be called in cpu_me, io_me, V, and end_me as long as there is still
 * more operations to perform.
 * 
 * If it this decided that this thread should continue to execute then this function returns immediately,
 * other wise it will signal the thread to execute next and then block itself until another call to 
 * schedule decides that this thread should run next.
 * 
 * @param thread_id the id of the calling thread
 */
void schedule(int thread_id);

/**
 * @brief Ticks the global clock, this should update current IO operations as well
 * 
 * @param num_ticks number of ticks to skip forward
 */
void tick(int num_ticks);


/**
 * @brief Add the specified ProcessControlBlock to the ready queue
 * 
 * Exact implementation is #scheduler_type dependent.
 * 
 * @param block the block to enqueue into the ready queue
 */
void add_to_ready_queue(ProgramControlBlock *block);

/**
 * @brief Gets the next process in line the be run in the ready queue.
 * 
 * The behavior of this function is dependent on the #scheduler_type.
 * 
 * @return ProgramControlBlock* 
 */
ProgramControlBlock *pick_from_ready_queue();

/**
 * @brief Gets the next process to arrive from the arrival queue.
 * 
 * @return ProgramControlBlock* the next process to arrive from the arrival queue.
 */
ProgramControlBlock *pick_from_arrival_queue();

/**
 * @brief Gets the a process that has just completed its IO operation. 
 * If such a process is available.
 * 
 * @return ProgramControlBlock* a process that just completed IO, if available. NULL otherwise
 */
ProgramControlBlock *pick_from_io_queue();

/**
 * @brief Gets how many ticks until the next process arrives
 * 
 * @return int ticks until the next process arrives, -1 if no process is available
 */
int next_arrival();

/**
 * @brief Gets how many ticks until the next process arrives
 * 
 * @return int ticks until the next process arrives, -1 if no process is available
 */
int next_arrival();

/**
 * @brief Gets how many ticks until the current IO operation is finished
 * 
 * @return int ticks until the current IO operation finishes, -1 if no IO is in progress
 */
int next_io_queue_completion();

/**
 * @brief DEBUG: print the ready queue
 */
void print_ready_queue();

/**
 * @brief DEBUG: print the future queue
 * 
 */
void print_future_queue();

/**
 * @brief DEBUG: print the io queue
 * 
 */
void print_io_queue();

/**
 * @brief Cleans up before exit.
 * 
 * Deallocates any memory allocated and deinitialize all data structures.
 * This should be called by the last thread just before exiting end_me()
 * 
 */
void clean_up();

/**
 * @brief total number of processes
 * 
 */
extern int num_processes;

/**
 * @brief number of started processes
 * 
 */
extern int num_started_processes;

/**
 * @brief number of running processes
 * 
 */
extern int num_running_processes;

/**
 * @brief Array holding ProgramControlBlocks of each thread.
 * 
 */
extern ProgramControlBlock *program_control_blocks;

/**
 * @brief global simulation time
 * 
 * Do not change this value except using the tick() function
 */
extern int wall_clock;

/**
 * @brief Mutex to ensure only one thread is active at any time.
 * 
 * All threads acquires this in their first call to startup_check().
 * They will temporarily relinquish it when blocked by their respective
 * ProgramControlBlockS::allowed_to_run conditional.
 * Finally they release it near the end of end_me()
 */
extern pthread_mutex_t execution_lock;

/**
 * @brief the arrival queue
 * 
 * This queue holds all threads that will arrive in the 
 * future according to the curent simulation time(wall_clock).
 * 
 * pick_from_arrival_queue() and next_arrival() operate on this queue.
 * 
 */
extern PriorityQueue arrival_queue;

/**
 * @brief the Ready queue(s)
 * 
 * A set of queues to hold threads waiting for their turn on the cpu.
 * The exact operations of this queue is dependent on the scheduler type
 * 
 * add_to_ready_queue() and pick_from_ready_queue() operates on this queue
 * 
 */
extern PriorityQueue * ready_queue;

/**
 * @brief the IO Queue
 * 
 * Holds threads that are preforming or waiting to perform io.
 * The head is the thread that is currently performing IO while
 * all other threads are waiting
 * 
 */
extern PriorityQueue io_queue;

/**
 * @brief all Semaphores
 * 
 */
extern Semaphore* semaphores;

/**
 * @brief The scheduler type
 * 
 * as given in init_scheduler()
 * 
 */
extern enum sch_type scheduler_type;

/**
 * @brief The currently executing thread on the cpu.
 * 
 * This is null if cpu is idle.
 * 
 */
extern ProgramControlBlock *current_cpu_program;


#endif