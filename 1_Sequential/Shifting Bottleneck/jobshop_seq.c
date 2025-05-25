// jobshop_seq_shifting_bottleneck.c
// Abordagem 3: Shifting Bottleneck simplificada

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_JOBS 100
#define MAX_MACHINES 100
#define MAX_OPS 100
#define INF 1000000

typedef struct {
    int machine;
    int duration;
    int start_time;
} Operation;

typedef struct {
    Operation ops[MAX_OPS];
    int num_ops;
} Job;

int find_bottleneck_machine(int num_machines, int *machine_loads) {
    int max_load = -1, bottleneck = -1;
    for (int i = 0; i < num_machines; i++) {
        if (machine_loads[i] > max_load) {
            max_load = machine_loads[i];
            bottleneck = i;
        }
    }
    return bottleneck;
}

int main() {
    FILE *input = fopen("../input/med100.jss", "r");
    FILE *output = fopen("../output/bottleneck_output.txt", "w");
    FILE *metrics = fopen("../output/bottleneck_metrics.txt", "w");
    if (!input || !output || !metrics) {
        perror("Erro ao abrir ficheiros");
        return 1;
    }

    clock_t start_time = clock();

    int num_jobs, num_machines;
    fscanf(input, "%d %d", &num_machines, &num_jobs);

    Job jobs[MAX_JOBS];
    int machine_available[MAX_MACHINES] = {0};
    int job_next_op[MAX_JOBS] = {0};
    int job_ready[MAX_JOBS] = {0};

    for (int j = 0; j < num_jobs; j++) {
        jobs[j].num_ops = num_machines;
        for (int o = 0; o < num_machines; o++) {
            fscanf(input, "%d %d", &jobs[j].ops[o].machine, &jobs[j].ops[o].duration);
            jobs[j].ops[o].start_time = -1;
        }
    }

    int total_ops = num_jobs * num_machines;
    int completed_ops = 0;

    while (completed_ops < total_ops) {
        int machine_load[MAX_MACHINES] = {0};

        for (int j = 0; j < num_jobs; j++) {
            int o = job_next_op[j];
            if (o >= jobs[j].num_ops) continue;
            int m = jobs[j].ops[o].machine;
            machine_load[m] += jobs[j].ops[o].duration;
        }

        int bottleneck = find_bottleneck_machine(num_machines, machine_load);

        int chosen_job = -1;
        int earliest_start = INF;

        for (int j = 0; j < num_jobs; j++) {
            int o = job_next_op[j];
            if (o >= jobs[j].num_ops) continue;

            Operation *op = &jobs[j].ops[o];
            if (op->machine != bottleneck) continue;

            int est = (job_ready[j] > machine_available[op->machine]) ? job_ready[j] : machine_available[op->machine];
            if (est < earliest_start) {
                earliest_start = est;
                chosen_job = j;
            }
        }

        if (chosen_job != -1) {
            int o = job_next_op[chosen_job];
            Operation *op = &jobs[chosen_job].ops[o];
            op->start_time = earliest_start;
            machine_available[op->machine] = earliest_start + op->duration;
            job_ready[chosen_job] = earliest_start + op->duration;
            job_next_op[chosen_job]++;
            completed_ops++;
        }
    }

    int makespan = 0;
    for (int j = 0; j < num_jobs; j++) {
        for (int o = 0; o < jobs[j].num_ops; o++) {
            int finish = jobs[j].ops[o].start_time + jobs[j].ops[o].duration;
            if (finish > makespan)
                makespan = finish;
        }
    }

    fprintf(output, "%d\n", makespan);
    for (int j = 0; j < num_jobs; j++) {
        for (int o = 0; o < jobs[j].num_ops; o++) {
            fprintf(output, "%d ", jobs[j].ops[o].start_time);
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
