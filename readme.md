# CMPSC 473 Project 1

[![CI status](https://github.com/PSU473/p1-2022-scheduler-semicolonTransistor/actions/workflows/ci.yml/badge.svg)](https://github.com/PSU473/p1-2022-scheduler-semicolonTransistor)

Jinyu Liu(liu.jinyu@psu.edu), Hongyu Guo(hbg5147@psu.edu)

## Main Data Structures

### Queues

The priority queue is the data structure all queues are based on. It is implemented on a linked list. It takes a priority argument in its enqueue function to allow flexibility. Entries with smaller priority numbers have higher priorities and ties are broken with the thread_id where lower ids have higher priority.

#### arrival_queue

The arrival_queue holds threads that will arrive in the future. Its entries are ordered by arrival time

#### ready_queue

The ready queue(s) holds threads that are waiting for the CPU. Its behavior changes depending on the type of CPU scheduler specified.

#### io_queue
The IO queue holds the threads that are performing or waiting to perform IO. It is FCFS and the head of the queue is the thread currently doing IO.

#### semaphore's blocked_queues
The blocked queue holds threads that are blocked by this semaphore. It is FCFS.

### ProgramControlBlock
The ProgramControlBlock are the block that 
contain information about each thread in the system. Most importantly, the allow_to_run conditional that is used 
to control the thread's execution.

### Semaphore
Contain the value of semaphore and queue of threads blocked by the semaphore. 

## Theory of Operation
The program mostly runs single-threaded with only one thread actively running at 
any time to make sure to race conditions occurs while operating on the various data 
structures and to make scheduling easy. Each thread is blocked using its own conditional in its ProgramControlBlock, this allows the schedule function to choose
which thread to execute next by signaling only the conditional blocking the specific thread. Once one thread is unblocked it runs until another call to schedule where control is transferred to another thread.

### The start_check() Function

It is the job of the start_check() function to get the disorganized threads into this state where only one is running at a time. It enqueues the calling thread onto the
arrival queue and blocks the calling thread on its conditional. The once the last 
thread calls start_check, start_check will make the first call to schedule and set in motion the scheduling operations.

### The schedule() Function

The schedule function is heart of the scheduler. It decides which thread to transfer
control to. The main goal of the schedule function is ensure that the wall_clock is
not advanced until every thread has completed all operations in the current tick. To
do this it priorities operations that do not advance the wall clock. It choose
threads in the following order of priority.

1.  Any thread in the arrival_queue that arrived at the current time.
2.  Any thread in the io_queue that completed IO at the current time.
3.  The thread in the ready_queue that is next in line for the CPU.

Once the schedule function finds the next thread to run according to the priority order above, It will transfer control to that thread. 

However it is possible that the CPU is idle and all threads are either blocked or will arrive in the 
future. In this case we would need to skip time forward. Since threads blocked by semaphores require 
other threads to execute to be come unstuck, we don't need to consider them when figuring out how 
much to skip time forward by. Thus we only need to consider the following.

- The next thread in the arrival_queue that will arrive at some point in the future
- The thread currently doing IO that will complete at some point in the future
  
If the schedule function can not find any thread to run at the current time, it will find the 
earlier of the two from the list above, skip time to when that event happens, and transfer control 
to that thread. At this point schedule should have found a thread to give control to, if it still 
can't find a thread, it means there is a deadlock or bug, so it will raise an assertion error and 
terminate the program.

### The cpu_me() function

It puts the thread on the ready_queue and then calls schedule. Once the scheduler decides it should have the CPU it will increment the wall clock by 1 by calling tick(1) to represent the 1 unit of time passes while it is using the CPU. it will return the current wall_time as required.

### The io_me() function

It puts the thread on the io_queue and then calls schedule. Once the IO is completed, the scheduler will release
the thread, and it will return the current wall_clock.

### The P() function

It will decrement the value of the specified semaphore. If the value is negative, it will put the calling thread on the blocked_queue of the semaphore and call schedule to transfer control to another thread. After either it deciding not to block or the thread gets released by a corresponding call to V(), it will return the current wall_time.

### The V() function
It will increment the value of the specified semaphore. 
If there are thread being blocked,
it will moved one thread from blocked_queue to arrival_queue,
 to be run next time schedule is called.

### The end_me() function
It will call schedule to transfer control to another thread, unless it is last thread.
If it is the last thread, it will clean up all memory allocations, and exit.