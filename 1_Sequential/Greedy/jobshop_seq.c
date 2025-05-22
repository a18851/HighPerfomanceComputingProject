// jobshop_seq_greedy.c
// Abordagem 1: Greedy sequencial simples

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    FILE *input = fopen("../input/exemplo.jss", "r");
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

    for (int j = 0; j < num_jobs; j++) {
        jobs[j].num_ops = num_machines;
        for (int o = 0; o < num_machines; o++) {
            fscanf(input, "%d %d", &jobs[j].ops[o].machine, &jobs[j].ops[o].duration);
        }
    }

    int makespan = 0;
    for (int j = 0; j < num_jobs; j++) {
        int current_time = 0;
        for (int o = 0; o < jobs[j].num_ops; o++) {
            int m = jobs[j].ops[o].machine;
            int ready_time = (current_time > machine_available[m]) ? current_time : machine_available[m];
            jobs[j].start_times[o] = ready_time;
            machine_available[m] = ready_time + jobs[j].ops[o].duration;
            current_time = ready_time + jobs[j].ops[o].duration;
            if (makespan < current_time) makespan = current_time;
        }
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