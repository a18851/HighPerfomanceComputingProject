// jobshop_par_prioridade.c
// Abordagem 2 Paralela: Prioridade por menor duração com pthreads

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define MAX_JOBS 100
#define MAX_MACHINES 100
#define MAX_OPS 100

typedef struct {
    int machine;
    int duration;
    int job_id;
    int op_index;
} Task;

typedef struct {
    Task tasks[MAX_OPS];
    int start_times[MAX_OPS];
    int num_ops;
    int job_id;
    int next_op;
} Job;

int num_jobs, num_machines;
Job jobs[MAX_JOBS];
int machine_available[MAX_MACHINES];
int job_ready[MAX_JOBS];
int makespan = 0;
pthread_mutex_t machine_mutex[MAX_MACHINES];
pthread_mutex_t makespan_mutex;

int compare_tasks(const void *a, const void *b) {
    Task *t1 = (Task *)a;
    Task *t2 = (Task *)b;
    return t1->duration - t2->duration;
}

void *process_task(void *arg) {
    Task *task = (Task *)arg;
    int j = task->job_id;
    int o = task->op_index;
    int m = task->machine;
    int d = task->duration;

    while (1) {
        if (o > 0 && jobs[j].start_times[o - 1] == -1) continue;

        pthread_mutex_lock(&machine_mutex[m]);
        int ready_time = (o == 0) ? 0 : jobs[j].start_times[o - 1] + jobs[j].tasks[o - 1].duration;
        int start = (ready_time > machine_available[m]) ? ready_time : machine_available[m];
        if (jobs[j].start_times[o] == -1) {
            jobs[j].start_times[o] = start;
            machine_available[m] = start + d;

            pthread_mutex_lock(&makespan_mutex);
            if (makespan < start + d) makespan = start + d;
            pthread_mutex_unlock(&makespan_mutex);

            pthread_mutex_unlock(&machine_mutex[m]);
            break;
        }
        pthread_mutex_unlock(&machine_mutex[m]);
    }
    return NULL;
}

int main() {
    FILE *input = fopen("../input/exemplo.jss", "r");
    FILE *output = fopen("../output/prioridade_par_output.txt", "w");
    FILE *metrics = fopen("../output/prioridade_par_metrics.txt", "w");
    if (!input || !output || !metrics) {
        perror("Erro ao abrir ficheiros");
        return 1;
    }

    clock_t start_time = clock();

    fscanf(input, "%d %d", &num_machines, &num_jobs);
    Task task_pool[MAX_JOBS * MAX_OPS];
    int task_count = 0;

    for (int j = 0; j < num_jobs; j++) {
        jobs[j].num_ops = num_machines;
        jobs[j].job_id = j;
        jobs[j].next_op = 0;
        for (int o = 0; o < num_machines; o++) {
            int m, d;
            fscanf(input, "%d %d", &m, &d);
            Task t = {m, d, j, o};
            jobs[j].tasks[o] = t;
            jobs[j].start_times[o] = -1;
            task_pool[task_count++] = t;
        }
    }

    qsort(task_pool, task_count, sizeof(Task), compare_tasks);
    memset(machine_available, 0, sizeof(machine_available));
    memset(job_ready, 0, sizeof(job_ready));
    for (int i = 0; i < num_machines; i++) pthread_mutex_init(&machine_mutex[i], NULL);
    pthread_mutex_init(&makespan_mutex, NULL);

    pthread_t threads[MAX_JOBS * MAX_OPS];
    for (int i = 0; i < task_count; i++) {
        pthread_create(&threads[i], NULL, process_task, (void *)&task_pool[i]);
    }

    for (int i = 0; i < task_count; i++) {
        pthread_join(threads[i], NULL);
    }

    int makespan_local = 0;
    fprintf(output, "%d\n", makespan);
    for (int j = 0; j < num_jobs; j++) {
        for (int o = 0; o < jobs[j].num_ops; o++) {
            fprintf(output, "%d ", jobs[j].start_times[o]);
        }
        fprintf(output, "\n");
    }

    clock_t end_time = clock();
    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    fprintf(metrics, "Tempo de execucao: %.4f segundos\n", elapsed);
    fprintf(metrics, "Makespan: %d\n", makespan);

    fclose(input);
    fclose(output);
    fclose(metrics);
    return 0;
}
