// jobshop_seq_prioridade.c
// Abordagem 2: Prioridade por menor duração de operação

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_JOBS 100
#define MAX_MACHINES 100
#define MAX_OPS 100

typedef struct
{
    int machine;
    int duration;
    int job_id;
    int op_index;
} Task;

typedef struct
{
    Task tasks[MAX_OPS];
    int num_ops;
    int next_op_index;
    int start_times[MAX_OPS];
} Job;

int compare_tasks(const void *a, const void *b)
{
    Task *t1 = (Task *)a;
    Task *t2 = (Task *)b;
    return t1->duration - t2->duration;
}

int main()
{
    FILE *input = fopen("../input/04.jss", "r");
    FILE *output = fopen("../output/prioridade_output.txt", "w");
    FILE *metrics = fopen("../output/prioridade_metrics.txt", "w");
    if (!input || !output || !metrics)
    {
        perror("Erro ao abrir ficheiros");
        return 1;
    }

    clock_t start_time = clock();

    int num_jobs, num_machines;
    fscanf(input, "%d %d", &num_machines, &num_jobs);

    Job jobs[MAX_JOBS];
    int machine_available[MAX_MACHINES] = {0};

    Task task_pool[MAX_JOBS * MAX_OPS];
    int task_count = 0;

    for (int j = 0; j < num_jobs; j++)
    {
        jobs[j].num_ops = num_machines;
        jobs[j].next_op_index = 0;
        for (int o = 0; o < num_machines; o++)
        {
            int m, d;
            fscanf(input, "%d %d", &m, &d);
            Task t = {m, d, j, o};
            jobs[j].tasks[o] = t;
            task_pool[task_count++] = t;
        }
    }

    qsort(task_pool, task_count, sizeof(Task), compare_tasks);

    int finished_ops[MAX_JOBS] = {0};
    int makespan = 0;

    for (int i = 0; i < task_count; i++)
    {
        Task t = task_pool[i];
        if (t.op_index != finished_ops[t.job_id])
            continue;

        int job_prev_time = 0;
        if (t.op_index > 0)
            job_prev_time = jobs[t.job_id].start_times[t.op_index - 1] + jobs[t.job_id].tasks[t.op_index - 1].duration;

        int start_time = (machine_available[t.machine] > job_prev_time) ? machine_available[t.machine] : job_prev_time;

        jobs[t.job_id].start_times[t.op_index] = start_time;
        machine_available[t.machine] = start_time + t.duration;
        if (makespan < machine_available[t.machine])
            makespan = machine_available[t.machine];

        finished_ops[t.job_id]++;
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

    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    fprintf(metrics, "Tempo de execucao: %.4f segundos\n", elapsed_time);
    fprintf(metrics, "Makespan: %d\n", makespan);

    fclose(input);
    fclose(output);
    fclose(metrics);
    return 0;
}
