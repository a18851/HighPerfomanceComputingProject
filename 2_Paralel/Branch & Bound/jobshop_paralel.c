#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

double getClock()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

#define MAX_JOBS 15
#define MAX_MACHINES 15
#define INF 999999
#define MAX_PERMUTATIONS 10000

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

int num_jobs, num_machines;
struct Operation jobs[MAX_JOBS][MAX_MACHINES];
int global_best_start[MAX_JOBS][MAX_MACHINES];
int global_best_makespan = INF;
pthread_mutex_t mutex;

typedef struct
{
    int permutation[MAX_JOBS];
} ThreadArg;

void copy_start_times(int dest[MAX_JOBS][MAX_MACHINES], int src[MAX_JOBS][MAX_MACHINES])
{
    for (int j = 0; j < num_jobs; j++)
        for (int m = 0; m < num_machines; m++)
            dest[j][m] = src[j][m];
}

int max(int a, int b)
{
    return (a > b) ? a : b;
}

void branch_and_bound(struct State state, int permutation[MAX_JOBS])
{
    int all_done = 1;
    for (int i = 0; i < num_jobs; i++)
    {
        int j = permutation[i];
        if (state.job_progress[j] < num_machines)
        {
            all_done = 0;
            break;
        }
    }
    if (all_done)
    {
        pthread_mutex_lock(&mutex);
        if (state.current_makespan < global_best_makespan)
        {
            global_best_makespan = state.current_makespan;
            copy_start_times(global_best_start, state.current_start);
        }
        pthread_mutex_unlock(&mutex);
        return;
    }

    for (int i = 0; i < num_jobs; i++)
    {
        int j = permutation[i];
        int step = state.job_progress[j];
        if (step < num_machines)
        {
            int m = jobs[j][step].machine;
            int d = jobs[j][step].duration;
            int start_time = max(state.job_available[j], state.machine_available[m]);
            int end_time = start_time + d;
            if (end_time >= global_best_makespan)
                continue;

            struct State next_state = state;
            next_state.job_progress[j]++;
            next_state.job_available[j] = end_time;
            next_state.machine_available[m] = end_time;
            next_state.current_start[j][step] = start_time;
            if (end_time > next_state.current_makespan)
                next_state.current_makespan = end_time;

            branch_and_bound(next_state, permutation);
        }
    }
}

void *thread_func(void *arg)
{
    ThreadArg *data = (ThreadArg *)arg;

    struct State initial_state = {0};
    for (int j = 0; j < MAX_JOBS; j++)
    {
        initial_state.job_progress[j] = 0;
        initial_state.job_available[j] = 0;
        for (int m = 0; m < MAX_MACHINES; m++)
            initial_state.current_start[j][m] = 0;
    }
    for (int m = 0; m < MAX_MACHINES; m++)
        initial_state.machine_available[m] = 0;
    initial_state.current_makespan = 0;

    branch_and_bound(initial_state, data->permutation);
    pthread_exit(NULL);
}

// Simple recursive permutation generator (limited to avoid explosion)
int perm_count = 0;
ThreadArg permutations[MAX_PERMUTATIONS];

void generate_permutations(int arr[], int l, int r)
{
    if (l == r)
    {
        for (int i = 0; i < num_jobs; i++)
            permutations[perm_count].permutation[i] = arr[i];
        perm_count++;
        return;
    }
    for (int i = l; i <= r; i++)
    {
        int tmp = arr[l];
        arr[l] = arr[i];
        arr[i] = tmp;
        generate_permutations(arr, l + 1, r);
        tmp = arr[l];
        arr[l] = arr[i];
        arr[i] = tmp;
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

    double start_time = getClock();
    pthread_mutex_init(&mutex, NULL);

    int base[MAX_JOBS];
    for (int i = 0; i < num_jobs; i++)
        base[i] = i;
    generate_permutations(base, 0, num_jobs - 1);

    pthread_t threads[MAX_PERMUTATIONS];
    for (int i = 0; i < perm_count; i++)
    {
        pthread_create(&threads[i], NULL, thread_func, (void *)&permutations[i]);
    }
    for (int i = 0; i < perm_count; i++)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex);
    double end_time = getClock();
    double elapsed = end_time - start_time;

    fprintf(output, "%d\n", global_best_makespan);
    for (int j = 0; j < num_jobs; j++)
    {
        for (int m = 0; m < num_machines; m++)
            fprintf(output, "%d ", global_best_start[j][m]);
        fprintf(output, "\n");
    }
    fprintf(metrics, "Tempo de execucao: %.4f segundos\n", elapsed);
    fprintf(metrics, "Makespan: %d\n", global_best_makespan);

    fclose(input);
    fclose(output);
    fclose(metrics);
    return 0;
}
