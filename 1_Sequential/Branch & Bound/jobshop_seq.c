
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_JOBS 15
#define MAX_MACHINES 15
#define INF 999999

struct Operation
{
    int machine;
    int duration;
};

struct State
{
    int job_progress[MAX_JOBS];
    int machine_available[MAX_MACHINES];
    int job_available[MAX_JOBS];
    int current_start[MAX_JOBS][MAX_MACHINES];
    int current_makespan;
};

// Global variables
int num_jobs, num_machines;
struct Operation jobs[MAX_JOBS][MAX_MACHINES];
int best_start[MAX_JOBS][MAX_MACHINES];
int best_makespan = INF;

int max(int a, int b)
{
    return (a > b) ? a : b;
}

void copy_start_times(int dest[MAX_JOBS][MAX_MACHINES], int src[MAX_JOBS][MAX_MACHINES])
{
    for (int j = 0; j < MAX_JOBS; j++)
        for (int m = 0; m < MAX_MACHINES; m++)
            dest[j][m] = src[j][m];
}

void branch_and_bound(struct State state)
{
    int all_done = 1;
    for (int j = 0; j < num_jobs; j++)
    {
        if (state.job_progress[j] < num_machines)
        {
            all_done = 0;
            break;
        }
    }
    if (all_done)
    {
        if (state.current_makespan < best_makespan)
        {
            best_makespan = state.current_makespan;
            copy_start_times(best_start, state.current_start);
        }
        return;
    }

    for (int j = 0; j < num_jobs; j++)
    {
        int step = state.job_progress[j];
        if (step < num_machines)
        {
            int m = jobs[j][step].machine;
            int d = jobs[j][step].duration;

            int start_time = max(state.job_available[j], state.machine_available[m]);
            int end_time = start_time + d;

            if (end_time >= best_makespan)
                continue;

            struct State next_state = state;

            next_state.job_progress[j]++;
            next_state.job_available[j] = end_time;
            next_state.machine_available[m] = end_time;
            next_state.current_start[j][step] = start_time;
            if (end_time > next_state.current_makespan)
                next_state.current_makespan = end_time;

            branch_and_bound(next_state);
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

    fscanf(input, "%d %d", &num_jobs, &num_machines);
    for (int j = 0; j < num_jobs; j++)
    {
        for (int m = 0; m < num_machines; m++)
        {
            fscanf(input, "%d %d", &jobs[j][m].machine, &jobs[j][m].duration);
        }
    }

    clock_t start_time = clock();

    struct State initial_state;
    for (int j = 0; j < MAX_JOBS; j++)
    {
        initial_state.job_progress[j] = 0;
        initial_state.job_available[j] = 0;
    }
    for (int m = 0; m < MAX_MACHINES; m++)
        initial_state.machine_available[m] = 0;
    for (int j = 0; j < MAX_JOBS; j++)
        for (int m = 0; m < MAX_MACHINES; m++)
            initial_state.current_start[j][m] = 0;

    initial_state.current_makespan = 0;

    branch_and_bound(initial_state);

    clock_t end_time = clock();
    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    fprintf(output, "%d\n", best_makespan);
    for (int j = 0; j < num_jobs; j++)
    {
        for (int m = 0; m < num_machines; m++)
            fprintf(output, "%d ", best_start[j][m]);
        fprintf(output, "\n");
    }
    fprintf(metrics, "Tempo de execucao: %.4f segundos\n", elapsed);
    fprintf(metrics, "Makespan: %d\n", best_makespan);

    fclose(input);
    fclose(output);
    fclose(metrics);
    return 0;
}
