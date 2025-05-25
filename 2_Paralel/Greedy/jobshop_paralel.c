// jobshop_par_greedy.c
// Abordagem 1 Paralela: Greedy com pthreads
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>

#define MAX_JOBS 100
#define MAX_MACHINES 100
#define MAX_OPS 100
#define MAX_THREADS 4

typedef struct
{
    int machine;
    int duration;
} Operation;

typedef struct
{
    Operation ops[MAX_OPS];
    int start_times[MAX_OPS];
    int num_ops;
    int job_id;
} Job;

int num_jobs, num_machines;
Job jobs[MAX_JOBS];
int machine_available[MAX_MACHINES] = {0};
int job_ready[MAX_JOBS] = {0};
int makespan = 0;

pthread_mutex_t machine_mutex[MAX_MACHINES];
pthread_mutex_t makespan_mutex = PTHREAD_MUTEX_INITIALIZER;

void *process_job(void *arg)
{
    Job *job = (Job *)arg;
    int current_time = 0;

    for (int o = 0; o < job->num_ops; o++)
    {
        int m = job->ops[o].machine;
        int d = job->ops[o].duration;

        pthread_mutex_lock(&machine_mutex[m]);
        int start_time = (current_time > machine_available[m]) ? current_time : machine_available[m];
        machine_available[m] = start_time + d;
        pthread_mutex_unlock(&machine_mutex[m]);

        job->start_times[o] = start_time;
        current_time = start_time + d;

        pthread_mutex_lock(&makespan_mutex);
        if (makespan < current_time)
            makespan = current_time;
        pthread_mutex_unlock(&makespan_mutex);
    }
    return NULL;
}

int main()
{
    FILE *input = fopen("../input/04.jss", "r");
    FILE *output = fopen("../output/greedy_par_output.txt", "w");
    FILE *metrics = fopen("../output/greedy_par_metrics.txt", "w");
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
        jobs[j].job_id = j;
        for (int o = 0; o < num_machines; o++)
        {
            fscanf(input, "%d %d", &jobs[j].ops[o].machine, &jobs[j].ops[o].duration);
        }
    }

    for (int i = 0; i < num_machines; i++)
        pthread_mutex_init(&machine_mutex[i], NULL);

    pthread_t threads[MAX_JOBS];
    for (int i = 0; i < num_jobs; i++)
    {
        pthread_create(&threads[i], NULL, process_job, (void *)&jobs[i]);
    }

    for (int i = 0; i < num_jobs; i++)
    {
        pthread_join(threads[i], NULL);
    }

    fprintf(output, "%d\n", makespan);
    for (int j = 0; j < num_jobs; j++)
    {
        for (int o = 0; o < jobs[j].num_ops; o++)
        {
            fprintf(output, "%d ", jobs[j].start_times[o]);
        }
        fprintf(output, "\n");
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    fprintf(metrics, "Tempo de execucao: %.4f segundos\n", elapsed);
    fprintf(metrics, "Makespan: %d\n", makespan);

    fclose(input);
    fclose(output);
    fclose(metrics);
    return 0;
}
