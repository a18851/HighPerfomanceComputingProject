// jobshop_seq_branch_bound.c
// Abordagem 4: Branch & Bound simples

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#define MAX_JOBS 10
#define MAX_MACHINES 10
#define MAX_OPS 10

typedef struct {
    int machine;
    int duration;
} Operation;

typedef struct {
    Operation ops[MAX_OPS];
    int start_times[MAX_OPS];
    int num_ops;
} Job;

int num_jobs, num_machines;
Job jobs[MAX_JOBS];
int best_makespan = INT_MAX;
int best_schedule[MAX_JOBS][MAX_OPS];

int machine_available[MAX_MACHINES];
int job_ready[MAX_JOBS];

void branch(int job_index, int op_index, int current_makespan) {
    if (job_index == num_jobs) {
        if (current_makespan < best_makespan) {
            best_makespan = current_makespan;
            for (int j = 0; j < num_jobs; j++)
                for (int o = 0; o < jobs[j].num_ops; o++)
                    best_schedule[j][o] = jobs[j].start_times[o];
        }
        return;
    }

    if (op_index == jobs[job_index].num_ops) {
        branch(job_index + 1, 0, current_makespan);
        return;
    }

    Operation *op = &jobs[job_index].ops[op_index];
    int start_time = job_ready[job_index] > machine_available[op->machine] ? job_ready[job_index] : machine_available[op->machine];
    int finish_time = start_time + op->duration;

    jobs[job_index].start_times[op_index] = start_time;
    int prev_machine_time = machine_available[op->machine];
    int prev_job_time = job_ready[job_index];
    machine_available[op->machine] = finish_time;
    job_ready[job_index] = finish_time;

    if (finish_time < best_makespan)
        branch(job_index, op_index + 1, finish_time > current_makespan ? finish_time : current_makespan);

    // backtrack
    machine_available[op->machine] = prev_machine_time;
    job_ready[job_index] = prev_job_time;
}

int main() {
    FILE *input = fopen("../input/exemplo.jss", "r");
    FILE *output = fopen("../output/bnb_output.txt", "w");
    FILE *metrics = fopen("../output/bnb_metrics.txt", "w");
    if (!input || !output || !metrics) {
        perror("Erro ao abrir ficheiros");
        return 1;
    }

    clock_t start = clock();

    fscanf(input, "%d %d", &num_machines, &num_jobs);

    for (int j = 0; j < num_jobs; j++) {
        jobs[j].num_ops = num_machines;
        for (int o = 0; o < num_machines; o++) {
            fscanf(input, "%d %d", &jobs[j].ops[o].machine, &jobs[j].ops[o].duration);
        }
    }

    memset(machine_available, 0, sizeof(machine_available));
    memset(job_ready, 0, sizeof(job_ready));
    branch(0, 0, 0);

    fprintf(output, "%d\n", best_makespan);
    for (int j = 0; j < num_jobs; j++) {
        for (int o = 0; o < jobs[j].num_ops; o++) {
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
