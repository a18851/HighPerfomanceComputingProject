
// jobshop_seq_branch_bound_optimized.c
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#define MAX_JOBS 105
#define MAX_MACHINES 105
#define MAX_OPS 105

typedef struct
{
    int machine;
    int duration;
} Operation;

typedef struct
{
    Operation ops[MAX_OPS];
    int start_times[MAX_OPS];
    int next_op; // index of the next operation to schedule
    int num_ops;
} Job;

int num_jobs, num_machines;
Job jobs[MAX_JOBS];
int best_makespan = INT_MAX;
int best_schedule[MAX_JOBS][MAX_OPS];

int machine_available[MAX_MACHINES];
int job_ready[MAX_JOBS];

void branch(int current_makespan)
{
    int all_done = 1;

    for (int j = 0; j < num_jobs; j++)
    {
        if (jobs[j].next_op < jobs[j].num_ops)
        {

            printf("job %d, next op %d, num ops %d \n", j, jobs[j].next_op, jobs[j].num_ops);
            all_done = 0;

            Operation *op = &jobs[j].ops[jobs[j].next_op];
            int start_time = job_ready[j] > machine_available[op->machine] ? job_ready[j] : machine_available[op->machine];
            int finish_time = start_time + op->duration;

            // Save state
            int prev_machine_time = machine_available[op->machine];
            int prev_job_time = job_ready[j];
            jobs[j].start_times[jobs[j].next_op] = start_time;
            machine_available[op->machine] = finish_time;
            job_ready[j] = finish_time;
            jobs[j].next_op++;

            // Recurse
            if (finish_time < best_makespan)
                branch(finish_time > current_makespan ? finish_time : current_makespan);

            // Backtrack
            jobs[j].next_op--;
            machine_available[op->machine] = prev_machine_time;
            job_ready[j] = prev_job_time;
        }
    }

    if (all_done)
    {
        if (current_makespan < best_makespan)
        {
            best_makespan = current_makespan;
            for (int j = 0; j < num_jobs; j++)
                for (int o = 0; o < jobs[j].num_ops; o++)
                    best_schedule[j][o] = jobs[j].start_times[o];
        }
    }
}

int main()
{
    FILE *input = fopen("../input/04.jss", "r");
    FILE *output = fopen("../output/bnb_output.txt", "w");
    FILE *metrics = fopen("../output/bnb_metrics.txt", "w");
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
        jobs[j].next_op = 0;
        for (int o = 0; o < num_machines; o++)
        {
            fscanf(input, "%d %d", &jobs[j].ops[o].machine, &jobs[j].ops[o].duration);
        }
    }

    memset(machine_available, 0, sizeof(machine_available));
    memset(job_ready, 0, sizeof(job_ready));
    branch(0);

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
