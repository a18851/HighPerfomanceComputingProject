#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_JOBS 10
#define MAX_MACHINES 10
#define MAX_OPERATIONS 1000
#define MAX_NODES 10000

int num_jobs, num_machines;
int operations[MAX_JOBS][MAX_MACHINES][2]; // [job][op][0:machine, 1:duration]
int best_makespan = 1e9;
int best_start[MAX_JOBS][MAX_MACHINES];

typedef struct
{
    int start_times[MAX_JOBS][MAX_MACHINES];
    int job_time[MAX_JOBS];
    int machine_time[MAX_MACHINES];
    int depth;
} Node;

Node job_pool[MAX_NODES];
int pool_size = 0;

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
            int scheduled_machine = operations[j][o][0];
            if (scheduled_machine != machine)
                continue;

            int scheduled_start = node->start_times[j][o];
            int scheduled_duration = operations[j][o][1];
            int scheduled_end = scheduled_start + scheduled_duration;
            int new_end = start + duration;

            if (!(new_end <= scheduled_start || start >= scheduled_end))
            {
                return 1; // conflict
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    FILE *input = fopen("../input/jj06.jss", "r");
    FILE *output = fopen("../output/bnb_output.txt", "w");
    FILE *metrics = fopen("../output/bnb_metrics.txt", "w");
    if (!input || !output || !metrics)
    {
        perror("Erro ao abrir ficheiros");
        return 1;
    }

    clock_t start_time = clock();
    read_input(input);

    Node root = {.depth = 0};
    for (int i = 0; i < MAX_MACHINES; i++)
        root.machine_time[i] = 0;
    for (int i = 0; i < MAX_JOBS; i++)
        root.job_time[i] = 0;
    for (int j = 0; j < MAX_JOBS; j++)
        for (int m = 0; m < MAX_MACHINES; m++)
            root.start_times[j][m] = -1;

    job_pool[pool_size++] = root;

    while (pool_size > 0)
    {
        Node node = job_pool[--pool_size];

        if (node.depth == num_jobs * num_machines)
        {
            int finish_time = 0;
            for (int i = 0; i < num_jobs; i++)
                if (node.job_time[i] > finish_time)
                    finish_time = node.job_time[i];
            if (finish_time < best_makespan)
            {
                best_makespan = finish_time;
                for (int j = 0; j < num_jobs; j++)
                    for (int m = 0; m < num_machines; m++)
                        best_start[j][m] = node.start_times[j][m];
            }
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
            {
                start++;
            }

            Node child = node;
            child.start_times[job][op] = start;
            child.job_time[job] = start + duration;
            child.machine_time[machine] = start + duration;
            child.depth++;

            if (pool_size < MAX_NODES)
                job_pool[pool_size++] = child;
        }
    }

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
