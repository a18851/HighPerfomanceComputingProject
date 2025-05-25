// jobshop_seq_greedy.c
// Abordagem 1: Greedy sequencial simples

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define MAX_JOBS 100
#define MAX_MACHINES 100
#define MAX_OPS 100

typedef struct {
    int machine;
    int duration;
} Operation;

typedef struct {
    Operation ops[MAX_OPS];
    int num_ops;
    int start_times[MAX_OPS];
} Job;

int main() {
    FILE *input = fopen("../input/04.jss", "r");
    FILE *output = fopen("../output/greedy_output.txt", "w");
    FILE *metrics = fopen("../output/greedy_metrics.txt", "w");

    if (!input || !output || !metrics) {
        perror("Erro ao abrir ficheiros");
        return 1;
    }

    clock_t start_time = clock();

    int num_jobs, num_machines;
    fscanf(input, "%d %d", &num_machines, &num_jobs);

    Job jobs[MAX_JOBS];
    int machine_available[MAX_MACHINES] = {0};
    int job_ready[MAX_JOBS] = {0};
    int ops_remaining[MAX_JOBS];
    int makespan = 0;

    for (int j = 0; j < num_jobs; j++)
    {
        jobs[j].num_ops = num_machines;
        ops_remaining[j] = 0;
        for (int o = 0; o < num_machines; o++)
        {
            fscanf(input, "%d %d", &jobs[j].ops[o].machine, &jobs[j].ops[o].duration);
        }
    }

    int total_ops = num_jobs * num_machines;

    for (int scheduled_ops = 0; scheduled_ops < total_ops; scheduled_ops++)
    {
        int best_job = -1;
        int best_start_time = INT_MAX;

        for (int j = 0; j < num_jobs; j++)
        {
            if (ops_remaining[j] < jobs[j].num_ops)
            {
                int op_index = ops_remaining[j];
                Operation *op = &jobs[j].ops[op_index];
                int ready_time = job_ready[j] > machine_available[op->machine] ? job_ready[j] : machine_available[op->machine];

                if (ready_time < best_start_time)
                {
                    best_start_time = ready_time;
                    best_job = j;
                }
            }
        }

        // Schedule the best operation
        int op_index = ops_remaining[best_job];
        Operation *op = &jobs[best_job].ops[op_index];
        jobs[best_job].start_times[op_index] = best_start_time;

        int finish_time = best_start_time + op->duration;
        job_ready[best_job] = finish_time;
        machine_available[op->machine] = finish_time;

        if (makespan < finish_time)
            makespan = finish_time;

        ops_remaining[best_job]++;
    }

    fprintf(output, "%d\n", makespan);
    for (int j = 0; j < num_jobs; j++) {
        for (int o = 0; o < jobs[j].num_ops; o++) {
            fprintf(output, "%d ", jobs[j].start_times[o]);
        }
        fprintf(output, "\n");
    }

    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    fprintf(metrics, "Tempo de execucao: %.8f segundos\n", elapsed_time);
    fprintf(metrics, "Makespan: %d\n", makespan);

    fclose(input);
    fclose(output);
    fclose(metrics);

    return 0;
}