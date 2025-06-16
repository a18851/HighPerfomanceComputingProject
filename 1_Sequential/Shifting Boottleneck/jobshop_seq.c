#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define MAX_JOBS 105
#define MAX_MACHINES 105
#define INF 999999

typedef struct
{
    int machine;
    int duration;
} Operation;

typedef struct
{
    int job;
    int op_index;
    int duration;
} MachineOp;

// Global Variables
int num_jobs, num_machines;
Operation jobs[MAX_JOBS][MAX_MACHINES];
MachineOp machine_ops[MAX_MACHINES][MAX_JOBS];
int machine_op_count[MAX_MACHINES] = {0};
int scheduled[MAX_MACHINES] = {0};

// For storing fixed machine sequences
int machine_sequence[MAX_MACHINES][MAX_JOBS];
int machine_sequence_len[MAX_MACHINES] = {0};

// Start times for output
int start_times[MAX_JOBS][MAX_MACHINES] = {0};
int final_makespan = 0;

double getClock()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// Brute-force scheduler for one machine
void permute(int n, MachineOp *ops, int *best_time, MachineOp *best_order)
{
    int indices[n];
    for (int i = 0; i < n; i++)
        indices[i] = i;

    while (1)
    {
        int job_tmp_ready[MAX_JOBS] = {0};
        int machine_time = 0;
        int tmp_start[MAX_JOBS][MAX_MACHINES] = {{0}};
        int time = 0;

        for (int i = 0; i < n; i++)
        {
            MachineOp op = ops[indices[i]];
            int prev_end = 0;
            if (op.op_index > 0)
            {
                prev_end = tmp_start[op.job][op.op_index - 1] + jobs[op.job][op.op_index - 1].duration;
            }
            int ready = job_tmp_ready[op.job] > machine_time ? job_tmp_ready[op.job] : machine_time;
            if (ready < prev_end)
                ready = prev_end;

            tmp_start[op.job][op.op_index] = ready;
            int end = ready + op.duration;
            job_tmp_ready[op.job] = end;
            machine_time = end;
            if (end > time)
                time = end;
        }

        if (time < *best_time)
        {
            *best_time = time;
            for (int i = 0; i < n; i++)
                best_order[i] = ops[indices[i]];
        }

        // Next permutation
        int i = n - 2;
        while (i >= 0 && indices[i] > indices[i + 1])
            i--;
        if (i < 0)
            break;
        int j = n - 1;
        while (indices[i] > indices[j])
            j--;
        int tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
        for (int a = i + 1, b = n - 1; a < b; a++, b--)
        {
            tmp = indices[a];
            indices[a] = indices[b];
            indices[b] = tmp;
        }
    }
}

void schedule_machine(int m)
{
    int n = machine_op_count[m];
    MachineOp best_order[MAX_JOBS];
    int best_time = INF;
    permute(n, machine_ops[m], &best_time, best_order);

    for (int i = 0; i < n; i++)
        machine_sequence[m][i] = best_order[i].job;
    machine_sequence_len[m] = n;

    scheduled[m] = 1;
}

// After all machine sequences are fixed, compute final schedule
void compute_schedule()
{
    int job_op_done[MAX_JOBS] = {0};
    int job_ready[MAX_JOBS] = {0};
    int machine_ready[MAX_MACHINES] = {0};

    int updated = 1;
    while (updated)
    {
        updated = 0;

        for (int m = 0; m < num_machines; m++)
        {
            for (int i = 0; i < machine_sequence_len[m]; i++)
            {
                int j = machine_sequence[m][i];
                int k = -1;
                for (int step = 0; step < num_machines; step++)
                {
                    if (jobs[j][step].machine == m)
                        k = step;
                }

                if (k < 0 || start_times[j][k] > 0)
                    continue;

                if (k > 0 && start_times[j][k - 1] == 0 && jobs[j][k - 1].duration > 0)
                    continue;

                int prev_end = (k == 0) ? 0 : start_times[j][k - 1] + jobs[j][k - 1].duration;
                int ready = job_ready[j] > machine_ready[m] ? job_ready[j] : machine_ready[m];
                if (ready < prev_end)
                    ready = prev_end;

                start_times[j][k] = ready;
                int end = ready + jobs[j][k].duration;
                job_ready[j] = end;
                machine_ready[m] = end;
                if (end > final_makespan)
                    final_makespan = end;

                updated = 1;
            }
        }
    }
}

void shifting_bottleneck()
{
    for (int step = 0; step < num_machines; step++)
    {
        int worst_machine = -1;
        int worst_time = -1;

        for (int m = 0; m < num_machines; m++)
        {
            if (scheduled[m])
                continue;
            MachineOp best_order[MAX_JOBS];
            int time = INF;
            permute(machine_op_count[m], machine_ops[m], &time, best_order);
            if (time > worst_time)
            {
                worst_time = time;
                worst_machine = m;
            }
        }

        if (worst_machine != -1)
        {
            schedule_machine(worst_machine);
        }
    }
    compute_schedule();
}

int main()
{
    FILE *input = fopen("../input/med100.jss", "r");
    FILE *output = fopen("../output/bnb_output.txt", "w");
    FILE *metrics = fopen("../output/bnb_metrics.txt", "w");
    if (!input || !output || !metrics)
    {
        perror("Erro ao abrir ficheiros");
        return 1;
    }

    fscanf(input, "%d %d", &num_jobs, &num_machines);
    for (int j = 0; j < num_jobs; j++)
    {
        for (int o = 0; o < num_machines; o++)
        {
            int m, d;
            fscanf(input, "%d %d", &m, &d);
            jobs[j][o].machine = m;
            jobs[j][o].duration = d;

            machine_ops[m][machine_op_count[m]].job = j;
            machine_ops[m][machine_op_count[m]].op_index = o;
            machine_ops[m][machine_op_count[m]].duration = d;
            machine_op_count[m]++;
        }
    }

    double start = getClock();
    shifting_bottleneck();
    double end = getClock();

    fprintf(output, "%d\n", final_makespan);
    for (int j = 0; j < num_jobs; j++)
    {
        for (int m = 0; m < num_machines; m++)
            fprintf(output, "%d ", start_times[j][m]);
        fprintf(output, "\n");
    }

    fprintf(metrics, "Tempo de execucao: %.4f segundos\n", end - start);
    fprintf(metrics, "Makespan: %d\n", final_makespan);

    fclose(input);
    fclose(output);
    fclose(metrics);
    return 0;
}
