// jobshop_par_bottleneck.c
// Abordagem 3 Paralela: Shifting Bottleneck com pthreads (simplificada)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>

#define MAX_JOBS 100
#define MAX_MACHINES 100
#define MAX_OPS 100
#define MAX_THREADS 4

typedef struct
{
    int machine;
    int duration;
    int start_time;
} Operation;

typedef struct
{
    Operation ops[MAX_OPS];
    int num_ops;
} Job;

int num_jobs, num_machines;
Job jobs[MAX_JOBS];
int job_next_op[MAX_JOBS];
int machine_available[MAX_MACHINES];
int job_ready[MAX_JOBS];
int makespan = 0;
pthread_mutex_t mutex;
int active_jobs;

int find_bottleneck_machine()
{
    int load[MAX_MACHINES] = {0};
    for (int j = 0; j < num_jobs; j++)
    {
        int o = job_next_op[j];
        if (o >= jobs[j].num_ops)
            continue;
        int m = jobs[j].ops[o].machine;
        load[m] += jobs[j].ops[o].duration;
    }

    int max_load = -1, bottleneck = -1;
    for (int m = 0; m < num_machines; m++)
    {
        if (load[m] > max_load)
        {
            max_load = load[m];
            bottleneck = m;
        }
    }
    return bottleneck;
}

void *schedule_bottleneck(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&mutex);
        if (active_jobs == 0)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        int m = find_bottleneck_machine();
        int chosen_job = -1;
        int earliest = INT_MAX;

        for (int j = 0; j < num_jobs; j++)
        {
            int o = job_next_op[j];
            if (o >= jobs[j].num_ops)
                continue;

            Operation *op = &jobs[j].ops[o];
            if (op->machine != m)
                continue;

            int est = job_ready[j] > machine_available[m] ? job_ready[j] : machine_available[m];
            if (est < earliest)
            {
                earliest = est;
                chosen_job = j;
            }
        }

        if (chosen_job != -1)
        {
            int o = job_next_op[chosen_job];
            Operation *op = &jobs[chosen_job].ops[o];
            op->start_time = earliest;
            machine_available[op->machine] = earliest + op->duration;
            job_ready[chosen_job] = earliest + op->duration;
            job_next_op[chosen_job]++;
            if (makespan < machine_available[op->machine])
                makespan = machine_available[op->machine];
        }
        else
        {
            active_jobs = 0; // No more work to do
        }

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main()
{
    FILE *input = fopen("../input/04.jss", "r");
    FILE *output = fopen("../output/bottleneck_par_output.txt", "w");
    FILE *metrics = fopen("../output/bottleneck_par_metrics.txt", "w");
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
            jobs[j].ops[o].start_time = -1;
        }
        job_next_op[j] = 0;
        job_ready[j] = 0;
    }
    memset(machine_available, 0, sizeof(machine_available));
    pthread_mutex_init(&mutex, NULL);
    active_jobs = num_jobs;

    pthread_t threads[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++)
    {
        pthread_create(&threads[i], NULL, schedule_bottleneck, NULL);
    }
    for (int i = 0; i < MAX_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    fprintf(output, "%d\n", makespan);
    for (int j = 0; j < num_jobs; j++)
    {
        for (int o = 0; o < jobs[j].num_ops; o++)
        {
            fprintf(output, "%d ", jobs[j].ops[o].start_time);
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
