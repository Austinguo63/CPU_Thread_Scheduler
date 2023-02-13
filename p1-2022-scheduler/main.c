#include <sys/stat.h>
#include <libgen.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "interface.h"

#define MAX_LINE_LEN 1024
FILE *gantt_file;

struct thread_struct
{
    pthread_t p_t;           // pthread identifier
    int tid;                 // tid
    char line[MAX_LINE_LEN]; // tid's operations
};

void *thread_start(void *);
int get_line_count(char *file_name);

// Main function
// Read input file and create threads accordingly
int main(int argc, char **argv)
{
    printf("%s: Hello Project 1!\n", __func__);
    if (argc != 3)
    {
        fprintf(stderr, "Not enough parameters specified. Usage: ./proj1 <scheduler_type> <input_file>\n");
        fprintf(stderr, "  Scheduler type: 0 - First Come, First Served\n");
        fprintf(stderr, "  Scheduler type: 1 - Shortest Remaining Time First\n");
        fprintf(stderr, "  Scheduler type: 2 - Multi-Level Feedback Queue\n");
        return -EINVAL;
    }

    // Get parameters
    int scheduler_type = atoi(argv[1]);
    int num_lines = get_line_count(argv[2]);
    if (num_lines <= 0)
    {
        fprintf(stderr, "%s: invalid input file.\n", __func__);
        return -EINVAL;
    }

    int num_threads = num_lines;
    printf("%s: Scheduler type: %d, number of threads: %d\n", __func__, scheduler_type, num_threads);

    // Allocate thread_struct
    struct thread_struct *threads;
    threads = (struct thread_struct *)malloc(sizeof(*threads) * num_threads);
    if (!threads)
    {
        perror("malloc() error");
        return errno;
    }
    memset(threads, 0, sizeof(*threads) * num_threads);

    // Read each line and save inside threads[].line
    FILE *fp = fopen(argv[2], "r");
    char *buf = (char *)malloc(sizeof(char) * MAX_LINE_LEN);
    for (int i = 0; i < num_threads; ++i)
    {
        if (fgets(buf, MAX_LINE_LEN, fp) == NULL)
        {
            perror("fgets() error");
            return errno;
        }
        if (buf[strlen(buf) - 1] = '\n')
            buf[strlen(buf) - 1] = '\0'; // remove newline

        strncpy(threads[i].line, buf, MAX_LINE_LEN);
    }
    free(buf);
    fclose(fp);

    // Open file for Gantt chart
    char temp[512] = {0};
    mkdir("output", 0755);
    strcat(temp, "output/gantt-");
    strcat(temp, argv[1]);
    strcat(temp, "-");
    strcat(temp, basename(argv[2]));
    gantt_file = fopen(temp, "w");
    if (gantt_file == NULL)
    {
        perror("fopen() error");
        return errno;
    }

    // Init scheduler
    init_scheduler(scheduler_type, num_threads);

    // Assign tid and create threads using threads[]
    int ret = 0;
    for (int i = 0; i < num_threads; ++i)
    {
        threads[i].tid = i;
        ret = pthread_create(&(threads[i].p_t), NULL, thread_start, &(threads[i]));
        if (ret)
        {
            fprintf(stderr, "%s: pthread_create() error!\n", __func__);
            return -EPERM;
        }
    }

    // Join threads
    for (int i = 0; i < num_threads; ++i)
    {
        ret = pthread_join(threads[i].p_t, NULL);
        if (ret)
        {
            fprintf(stderr, "%s: pthread_join() error!\n", __func__);
            return -EPERM;
        }
    }

    fclose(gantt_file);
    free(threads);

    printf("%s: Output file: %s\n", __func__, temp);
    printf("%s: Bye!\n", __func__);
    return 0;
}

// Thread starting point
// Independently read each line and call C/I/P/V/E
void *thread_start(void *arg)
{
    struct thread_struct *my_info = (struct thread_struct *)arg;
    int tid = my_info->tid;

    // read tokens
    char *token = NULL;
    char delim[3] = "\t ";
    char *line = my_info->line;
    char *saveptr;

    // arrival time
    token = strtok_r(my_info->line, delim, &saveptr);
    float arrival_time = atof(token);

    // the first operation (C/I/P/V) call from this thread is the arrival time in input file
    float schedule_time = arrival_time;

    // tid
    token = strtok_r(NULL, delim, &saveptr);
    if (tid != atoi(token))
    {
        fprintf(stderr, "%s: tid: %d, incorrect tid\n", __func__, tid);
        exit(EXIT_FAILURE);
    }

    // loop until 'E'
    token = strtok_r(NULL, delim, &saveptr);
    while (token)
    {
        // save the return value (time) of C/I/P/V
        int ret_time = 0;

        // parse token
        if (token[0] == 'C')
        {
            int duration = atoi(&(token[1]));
            while (duration >= 0)
            {
                ret_time = cpu_me(schedule_time, tid, duration);
                // return from cpu_me()
                if (duration > 0)
                    // only print when CPU is actually requested
                    // (if duration is 0, we are just notifying the scheduler)
                    // this tid had cpu from 'ret_time-1' to 'ret_time'
                    fprintf(gantt_file, "%3d~%3d: T%d, CPU\n", ret_time - 1, ret_time, tid);

                // values for the next cpu_me() call
                schedule_time = ret_time;
                duration = duration - 1;
            }
        }
        else if (token[0] == 'I')
        {
            int duration = atoi(&(token[1]));
            ret_time = io_me(schedule_time, tid, duration);
            // return from io_me()
            // this tid finished IO at time 'ret_time'
            fprintf(gantt_file, "   ~%3d: T%d, Return from IO\n", ret_time, tid);
        }
        else if (token[0] == 'P')
        {
            int sem_id = atoi(&(token[1]));
            ret_time = P(schedule_time, tid, sem_id);
            // return from P()
            // this tid finished P at time 'ret_time'
            fprintf(gantt_file, "   ~%3d: T%d, Return from P%d\n", ret_time, tid, sem_id);
        }
        else if (token[0] == 'V')
        {
            int sem_id = atoi(&(token[1]));
            ret_time = V(schedule_time, tid, sem_id);
            // return from V()
            // this tid finished V at time 'ret_time'
            fprintf(gantt_file, "   ~%3d: T%d, Return from V%d\n", ret_time, tid, sem_id);
        }
        else if (token[0] == 'E')
        {
            // this thread is finished, notify scheduler
            end_me(tid);

            // end this thread normally
            return NULL;
        }
        else
        {
            fprintf(stderr, "%s: Error, tid: %d, invalid token: %c%c\n", __func__, tid, token[0], token[1]);
            exit(EXIT_FAILURE);
        }

        // get next token
        token = strtok_r(NULL, delim, &saveptr);

        // call the next operation without any time delay
        schedule_time = ret_time;
    }
    
    // No 'E' found in input file
    fprintf(stderr, "%s: Error, tid: %d, thread finished without 'E' operation\n", __func__, tid);
    exit(EXIT_FAILURE);
}

// From file_name, get the number of lines and do error check
int get_line_count(char *file_name)
{
    FILE *fp = fopen(file_name, "r");
    if (!fp)
    {
        perror("fopen() error");
        return -ENOENT;
    }

    int num_lines = 0;
    char arrival_time[512];
    int temp;
    char *buf = (char *)malloc(sizeof(char) * MAX_LINE_LEN);
    while (fgets(buf, MAX_LINE_LEN, fp) != NULL)
    {
        // Check tid of the input file. tid starts from 0
        sscanf(buf, "%s %d", arrival_time, &temp);
        if (temp != num_lines)
        {
            return -EINVAL;
        }
        ++num_lines;
    }
    free(buf);

    fclose(fp);
    return num_lines;
}