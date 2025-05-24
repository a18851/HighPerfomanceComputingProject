// jobshop_par_bnb_optimized.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>

#define MAX_JOBS 105
#define MAX_MACHINES 105
#define MAX_OPS 105
#define MAX_THREADS 2

typedef struct
{
    int machine;
    int duration;
} Operation;

typedef struct
{
    Operation ops[MAX_OPS];
    int start_times[MAX_OPS];
    int next_op;
    int num_ops;
} Job;

int num_jobs, num_machines;
Job jobs[MAX_JOBS];
int best_makespan = INT_MAX;
int best_schedule[MAX_JOBS][MAX_OPS];
pthread_mutex_t best_mutex;

void branch(Job local_jobs[MAX_JOBS], int machine_available[MAX_MACHINES], int job_ready[MAX_JOBS], int current_makespan)
{
    int all_done = 1;

    for (int j = 0; j < num_jobs; j++)
    {
        if (local_jobs[j].next_op < local_jobs[j].num_ops)
        {
            all_done = 0;

            Operation *op = &local_jobs[j].ops[local_jobs[j].next_op];
            int start_time = job_ready[j] > machine_available[op->machine] ? job_ready[j] : machine_available[op->machine];
            int finish_time = start_time + op->duration;

            // Save state
            int prev_machine_time = machine_available[op->machine];
            int prev_job_time = job_ready[j];
            local_jobs[j].start_times[local_jobs[j].next_op] = start_time;
            machine_available[op->machine] = finish_time;
            job_ready[j] = finish_time;
            local_jobs[j].next_op++;

            if (finish_time < best_makespan)
                branch(local_jobs, machine_available, job_ready, finish_time > current_makespan ? finish_time : current_makespan);

            // Backtrack
            local_jobs[j].next_op--;
            machine_available[op->machine] = prev_machine_time;
            job_ready[j] = prev_job_time;
        }
    }

    if (all_done)
    {
        pthread_mutex_lock(&best_mutex);
        if (current_makespan < best_makespan)
        {
            best_makespan = current_makespan;
            for (int j = 0; j < num_jobs; j++)
                for (int o = 0; o < local_jobs[j].num_ops; o++)
                    best_schedule[j][o] = local_jobs[j].start_times[o];
        }
        pthread_mutex_unlock(&best_mutex);
    }
}

void *thread_branch(void *arg)
{
    int id = *(int *)arg;
    Job local_jobs[MAX_JOBS];
    int local_job_ready[MAX_JOBS] = {0};
    int local_machine_available[MAX_MACHINES] = {0};

    memcpy(local_jobs, jobs, sizeof(jobs));
    for (int j = 0; j < num_jobs; j++)
        local_jobs[j].next_op = 0;

    branch(local_jobs, local_machine_available, local_job_ready, 0);
    return NULL;
}

int main()
{
    FILE *input = fopen("../input/04.jss", "r");
    FILE *output = fopen("../output/bnb_par_output.txt", "w");
    FILE *metrics = fopen("../output/bnb_par_metrics.txt", "w");
    if (!input || !output || !metrics)
    {
        perror("Erro ao abrir ficheiros");
        return 1;
    }

    clock_t start = clock();

    fscanf(input, "%d %d", &num_machines, &num_jobs);
    for (int j = 0; j < num_jobs; j++)
    {
        jobs[j].num_ops = num_machines;
        for (int o = 0; o < num_machines; o++)
        {
            fscanf(input, "%d %d", &jobs[j].ops[o].machine, &jobs[j].ops[o].duration);
        }
        jobs[j].next_op = 0;
    }

    pthread_mutex_init(&best_mutex, NULL);
    pthread_t threads[MAX_THREADS];
    int thread_ids[MAX_THREADS];

    for (int i = 0; i < MAX_THREADS; i++)
    {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_branch, &thread_ids[i]);
    }

    for (int i = 0; i < MAX_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    fprintf(output, "%d\n", best_makespan);
    for (int j = 0; j < num_jobs; j++)
    {
        for (int o = 0; o < jobs[j].num_ops; o++)
        {
            fprintf(output, "%d ", best_schedule[j][o]);
        }
        fprintf(output, "\n");
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    fprintf(metrics, "Tempo de execucao: %.4f segundos\n", elapsed);
    fprintf(metrics, "Makespan: %d\n", best_makespan);

    fclose(input);
    fclose(output);
    fclose(metrics);
    return 0;
}
