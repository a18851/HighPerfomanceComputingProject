#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#ifdef _OPENMP
#include <omp.h>
#define getClock() omp_get_wtime()
#else
#include <time.h>
#define getClock() ((double)clock() / CLOCKS_PER_SEC)
#endif

#define MAX_JOBS 8
#define MAX_MACHINES 8

// Global problem data
int num_jobs, num_machines;
int job_machine[MAX_JOBS][MAX_MACHINES];
int job_duration[MAX_JOBS][MAX_MACHINES];

// Best solution tracking
int best_makespan;
int best_schedule[MAX_JOBS][MAX_MACHINES];
long long nodes_explored = 0;

// Precomputed data for faster bounds
int job_remaining_time[MAX_JOBS][MAX_MACHINES + 1];

#ifdef _OPENMP
omp_lock_t best_lock;
#endif

void read_input()
{
    FILE *input = fopen("../input/05.jss", "r");
    if (!input)
    {
        printf("ERRO: Arquivo input/05.jss nao encontrado\n");
        exit(1);
    }

    fscanf(input, "%d %d", &num_jobs, &num_machines);
    printf("Problema: %d jobs, %d machines\n", num_jobs, num_machines);

    for (int j = 0; j < num_jobs; j++)
    {
        for (int op = 0; op < num_machines; op++)
        {
            fscanf(input, "%d %d", &job_machine[j][op], &job_duration[j][op]);
        }
    }
    fclose(input);

    // Precompute remaining times for each job from each operation
    for (int j = 0; j < num_jobs; j++)
    {
        job_remaining_time[j][num_machines] = 0;
        for (int op = num_machines - 1; op >= 0; op--)
        {
            job_remaining_time[j][op] = job_remaining_time[j][op + 1] + job_duration[j][op];
        }
    }

    printf("\nDados do problema:\n");
    for (int j = 0; j < num_jobs; j++)
    {
        printf("Job %d: ", j);
        for (int op = 0; op < num_machines; op++)
        {
            printf("(M%d,%d) ", job_machine[j][op], job_duration[j][op]);
        }
        printf("\n");
    }
    printf("\n");
}

// Get initial upper bound using simple heuristic
int get_initial_upper_bound()
{
    int temp_job_completion[MAX_JOBS] = {0};
    int temp_machine_completion[MAX_MACHINES] = {0};

    for (int j = 0; j < num_jobs; j++)
    {
        for (int op = 0; op < num_machines; op++)
        {
            int machine = job_machine[j][op];
            int duration = job_duration[j][op];

            int start_time = (temp_job_completion[j] > temp_machine_completion[machine]) ? temp_job_completion[j] : temp_machine_completion[machine];

            temp_job_completion[j] = start_time + duration;
            temp_machine_completion[machine] = start_time + duration;
        }
    }

    int makespan = 0;
    for (int j = 0; j < num_jobs; j++)
    {
        if (temp_job_completion[j] > makespan)
            makespan = temp_job_completion[j];
    }

    return makespan;
}

// Improved lower bound calculation
int calculate_improved_lower_bound(int job_completion[], int machine_completion[], int job_next_op[])
{
    int max_bound = 0;

    // Job-based bound using precomputed remaining times
    for (int j = 0; j < num_jobs; j++)
    {
        int job_bound = job_completion[j] + job_remaining_time[j][job_next_op[j]];
        if (job_bound > max_bound)
            max_bound = job_bound;
    }

    // Machine-based bound
    for (int m = 0; m < num_machines; m++)
    {
        int remaining_work = 0;
        for (int j = 0; j < num_jobs; j++)
        {
            for (int op = job_next_op[j]; op < num_machines; op++)
            {
                if (job_machine[j][op] == m)
                {
                    remaining_work += job_duration[j][op];
                }
            }
        }
        int machine_bound = machine_completion[m] + remaining_work;
        if (machine_bound > max_bound)
            max_bound = machine_bound;
    }

    return max_bound;
}

int all_jobs_complete(int job_next_op[])
{
    for (int j = 0; j < num_jobs; j++)
    {
        if (job_next_op[j] < num_machines)
            return 0;
    }
    return 1;
}

// Update best solution (thread-safe)
void update_best_solution(int schedule[MAX_JOBS][MAX_MACHINES], int makespan)
{
#ifdef _OPENMP
    omp_set_lock(&best_lock);
#endif

    if (makespan < best_makespan)
    {
        best_makespan = makespan;
        for (int j = 0; j < num_jobs; j++)
        {
            for (int op = 0; op < num_machines; op++)
            {
                best_schedule[j][op] = schedule[j][op];
            }
        }

#ifdef _OPENMP
        printf("Nova melhor solucao: makespan = %d (thread %d, nos: %lld)\n",
               best_makespan, omp_get_thread_num(), nodes_explored);
#else
        printf("Nova melhor solucao: makespan = %d (nos: %lld)\n",
               best_makespan, nodes_explored);
#endif
    }

#ifdef _OPENMP
    omp_unset_lock(&best_lock);
#endif
}

// Main Branch and Bound function with improvements
void branch_and_bound(int schedule[MAX_JOBS][MAX_MACHINES],
                      int job_completion[],
                      int machine_completion[],
                      int job_next_op[],
                      int depth)
{
#ifdef _OPENMP
#pragma omp atomic
#endif
    nodes_explored++;

// Progress indicator (less frequent for parallel)
#ifdef _OPENMP
    if (omp_get_thread_num() == 0 && nodes_explored % 5000000 == 0)
    {
        printf("Nos explorados: %lld, melhor makespan: %d\n", nodes_explored, best_makespan);
    }
#else
    if (nodes_explored % 5000000 == 0)
    {
        printf("Nos explorados: %lld, melhor makespan: %d\n", nodes_explored, best_makespan);
    }
#endif

    if (all_jobs_complete(job_next_op))
    {
        int makespan = 0;
        for (int j = 0; j < num_jobs; j++)
        {
            if (job_completion[j] > makespan)
                makespan = job_completion[j];
        }
        update_best_solution(schedule, makespan);
        return;
    }

    // Improved lower bound pruning
    int lower_bound = calculate_improved_lower_bound(job_completion, machine_completion, job_next_op);
    if (lower_bound >= best_makespan)
    {
        return;
    }

    // Depth limit
    if (depth > num_jobs * num_machines)
    {
        return;
    }

    // Collect available operations and apply light ordering
    typedef struct
    {
        int job;
        int remaining_time;
        int duration;
        int earliest_start;
    } JobInfo;

    JobInfo available_jobs[MAX_JOBS];
    int num_available = 0;

    for (int j = 0; j < num_jobs; j++)
    {
        if (job_next_op[j] < num_machines)
        {
            int op = job_next_op[j];
            int machine = job_machine[j][op];
            int duration = job_duration[j][op];
            int earliest_start = (job_completion[j] > machine_completion[machine]) ? job_completion[j] : machine_completion[machine];

            available_jobs[num_available].job = j;
            available_jobs[num_available].remaining_time = job_remaining_time[j][op];
            available_jobs[num_available].duration = duration;
            available_jobs[num_available].earliest_start = earliest_start;
            num_available++;
        }
    }

    // Light ordering: prefer jobs with more remaining work
    for (int i = 0; i < num_available - 1; i++)
    {
        for (int k = i + 1; k < num_available; k++)
        {
            if (available_jobs[k].remaining_time > available_jobs[i].remaining_time)
            {
                JobInfo temp = available_jobs[i];
                available_jobs[i] = available_jobs[k];
                available_jobs[k] = temp;
            }
        }
    }

// Parallelize only at shallow depths to avoid overhead
#ifdef _OPENMP
#pragma omp parallel for if (depth <= 2 && num_available > 1) schedule(dynamic)
#endif
    for (int idx = 0; idx < num_available; idx++)
    {
        int j = available_jobs[idx].job;
        int op = job_next_op[j];
        int machine = job_machine[j][op];
        int duration = job_duration[j][op];
        int start_time = available_jobs[idx].earliest_start;
        int end_time = start_time + duration;

        // Create new state (local copies for thread safety)
        int new_schedule[MAX_JOBS][MAX_MACHINES];
        int new_job_completion[MAX_JOBS];
        int new_machine_completion[MAX_MACHINES];
        int new_job_next_op[MAX_JOBS];

        // Copy current state
        for (int jj = 0; jj < num_jobs; jj++)
        {
            new_job_completion[jj] = job_completion[jj];
            new_job_next_op[jj] = job_next_op[jj];
            for (int oo = 0; oo < num_machines; oo++)
            {
                new_schedule[jj][oo] = schedule[jj][oo];
            }
        }
        for (int m = 0; m < num_machines; m++)
        {
            new_machine_completion[m] = machine_completion[m];
        }

        // Apply the operation
        new_schedule[j][op] = start_time;
        new_job_completion[j] = end_time;
        new_machine_completion[machine] = end_time;
        new_job_next_op[j]++;

        // Recursive call
        branch_and_bound(new_schedule, new_job_completion, new_machine_completion,
                         new_job_next_op, depth + 1);
    }
}

int main()
{
    nodes_explored = 0;

    read_input();

#ifdef _OPENMP
    omp_init_lock(&best_lock);
    printf("=== IMPROVED PARALLEL BRANCH AND BOUND ===\n");
    printf("Threads disponiveis: %d\n", omp_get_max_threads());
#else
    printf("=== IMPROVED BRANCH AND BOUND PARA JOB SHOP ===\n");
#endif

    // Get better initial upper bound
    best_makespan = get_initial_upper_bound();
    printf("Upper bound inicial (heuristica): %d\n", best_makespan);

    // Initialize arrays
    int schedule[MAX_JOBS][MAX_MACHINES];
    int job_completion[MAX_JOBS];
    int machine_completion[MAX_MACHINES];
    int job_next_op[MAX_JOBS];

    for (int j = 0; j < num_jobs; j++)
    {
        job_completion[j] = 0;
        job_next_op[j] = 0;
        for (int op = 0; op < num_machines; op++)
        {
            schedule[j][op] = -1;
            best_schedule[j][op] = -1;
        }
    }
    for (int m = 0; m < num_machines; m++)
    {
        machine_completion[m] = 0;
    }

    printf("Iniciando Improved Branch and Bound...\n");

    clock_t start_time = clock();
    double wall_start = getClock();

    // Start the search
    branch_and_bound(schedule, job_completion, machine_completion, job_next_op, 0);

    clock_t end_time = clock();
    double wall_end = getClock();
    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    double wall_elapsed = wall_end - wall_start;

#ifdef _OPENMP
    omp_destroy_lock(&best_lock);
#endif

    // Write results
    FILE *output = fopen("../output/bnb_output.txt", "w");
    if (output)
    {
        fprintf(output, "%d\n", best_makespan);
        for (int j = 0; j < num_jobs; j++)
        {
            for (int op = 0; op < num_machines; op++)
            {
                fprintf(output, "%d ", best_schedule[j][op]);
            }
            fprintf(output, "\n");
        }
        fclose(output);
    }

    FILE *metrics = fopen("../output/bnb_metrics.txt", "w");
    if (metrics)
    {
        fprintf(metrics, "Tempo de execucao (CPU): %.4f segundos\n", elapsed);
        fprintf(metrics, "Tempo de execucao (Wall): %.4f segundos\n", wall_elapsed);
        fprintf(metrics, "Makespan: %d\n", best_makespan);
        fprintf(metrics, "Nos explorados: %lld\n", nodes_explored);
#ifdef _OPENMP
        fprintf(metrics, "Threads utilizadas: %d\n", omp_get_max_threads());
        fprintf(metrics, "Speedup: %.2fx\n", elapsed > 0 ? elapsed / wall_elapsed : 1.0);
#endif
        fclose(metrics);
    }

    printf("\n=== RESULTADOS ===\n");
    printf("Melhor makespan: %d\n", best_makespan);
    printf("Tempo de execucao: %.4f segundos\n", wall_elapsed);
    printf("Nos explorados: %lld\n", nodes_explored);
#ifdef _OPENMP
    printf("Threads: %d, Speedup: %.2fx\n", omp_get_max_threads(),
           elapsed > 0 ? elapsed / wall_elapsed : 1.0);
#endif

    printf("\nEscalonamento otimo:\n");
    for (int j = 0; j < num_jobs; j++)
    {
        printf("Job %d: ", j);
        for (int op = 0; op < num_machines; op++)
        {
            if (best_schedule[j][op] >= 0)
            {
                printf("Op%d(M%d,t=%d->%d) ", op, job_machine[j][op],
                       best_schedule[j][op], best_schedule[j][op] + job_duration[j][op]);
            }
        }
        printf("\n");
    }

    return 0;
}