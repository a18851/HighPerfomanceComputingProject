#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#define MAX_JOBS 101
#define MAX_MACHINES 101
#define MAX_OPERATIONS 10001
#define MAX_NODES 100000

int num_jobs, num_machines;
int operations[MAX_JOBS][MAX_MACHINES][2];
int best_makespan = 1e9;
int best_start[MAX_JOBS][MAX_MACHINES];

pthread_mutex_t mutex;

typedef struct
{
    int start_times[MAX_JOBS][MAX_MACHINES];
    int job_time[MAX_JOBS];
    int machine_time[MAX_MACHINES];
    int depth;
} Node;

Node job_pool[MAX_NODES];
int front = 0, rear = 0;

void read_input(FILE *input)
{
    fscanf(input, "%d %d", &num_jobs, &num_machines);
    for (int i = 0; i < num_jobs; i++)
        for (int j = 0; j < num_machines; j++)
            fscanf(input, "%d %d", &operations[i][j][0], &operations[i][j][1]);
}

int is_conflict(Node *node, int machine, int start, int duration)
{
    for (int j = 0; j < num_jobs; j++)
    {
        for (int o = 0; o < num_machines; o++)
        {
            if (node->start_times[j][o] == -1)
                continue;
            if (operations[j][o][0] != machine)
                continue;
            int scheduled_start = node->start_times[j][o];
            int scheduled_duration = operations[j][o][1];
            int scheduled_end = scheduled_start + scheduled_duration;
            int new_end = start + duration;
            if (!(new_end <= scheduled_start || start >= scheduled_end))
            {
                return 1;
            }
        }
    }
    return 0;
}

void *branch_and_bound(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&mutex);
        if (front == rear)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }
        Node node = job_pool[front++];
        pthread_mutex_unlock(&mutex);

        if (node.depth == num_jobs * num_machines)
        {
            int finish_time = 0;
            for (int i = 0; i < num_jobs; i++)
                if (node.job_time[i] > finish_time)
                    finish_time = node.job_time[i];

            pthread_mutex_lock(&mutex);
            if (finish_time < best_makespan)
            {
                best_makespan = finish_time;
                for (int j = 0; j < num_jobs; j++)
                    for (int m = 0; m < num_machines; m++)
                        best_start[j][m] = node.start_times[j][m];
            }
            pthread_mutex_unlock(&mutex);
        }
        else
        {
            int job = node.depth / num_machines;
            int op = node.depth % num_machines;
            int machine = operations[job][op][0];
            int duration = operations[job][op][1];

            int earliest = node.job_time[job];
            int start = earliest;
            while (is_conflict(&node, machine, start, duration))
                start++;

            int estimated_end = start + duration;
            pthread_mutex_lock(&mutex);
            if (estimated_end >= best_makespan)
            {
                pthread_mutex_unlock(&mutex);
                continue;
            }
            pthread_mutex_unlock(&mutex);

            Node child = node;
            child.start_times[job][op] = start;
            child.job_time[job] = estimated_end;
            child.machine_time[machine] = estimated_end;
            child.depth++;

            pthread_mutex_lock(&mutex);
            if (rear < MAX_NODES)
                job_pool[rear++] = child;
            pthread_mutex_unlock(&mutex);
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    FILE *input = fopen("../input/04.jss", "r");
    FILE *output = fopen("../output/bnb_output.txt", "w");
    FILE *metrics = fopen("../output/bnb_metrics.txt", "w");
    if (!input || !output || !metrics)
    {
        perror("Erro ao abrir ficheiros");
        return 1;
    }

    clock_t start_time = clock();
    pthread_mutex_init(&mutex, NULL);
    read_input(input);

    Node root = {.depth = 0};
    memset(root.job_time, 0, sizeof(root.job_time));
    memset(root.machine_time, 0, sizeof(root.machine_time));
    for (int j = 0; j < MAX_JOBS; j++)
        for (int m = 0; m < MAX_MACHINES; m++)
            root.start_times[j][m] = -1;

    job_pool[rear++] = root;

    int thread_count = 8; // Fixed or changed here
    pthread_t threads[thread_count];
    for (int t = 0; t < thread_count; t++)
        pthread_create(&threads[t], NULL, branch_and_bound, NULL);
    for (int t = 0; t < thread_count; t++)
        pthread_join(threads[t], NULL);

    pthread_mutex_destroy(&mutex);

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
