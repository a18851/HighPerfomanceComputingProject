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
#define MAX_NODES_PER_THREAD 200000000
#define MAX_TOTAL_NODES 500000000

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

// Global time tracking
double global_start_time = 0;

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
// Obtem o limite superior inicial;
int get_initial_upper_bound()
{
    int temp_job_completion[MAX_JOBS] = {0};
    int temp_machine_completion[MAX_MACHINES] = {0};

    // For por cada job
    for (int j = 0; j < num_jobs; j++)
    {
        // For por cada operação dentro do job
        for (int op = 0; op < num_machines; op++)
        {
            // Obtem a máquina e a duração da operação
            int machine = job_machine[j][op];
            int duration = job_duration[j][op];

            int start_time = (temp_job_completion[j] > temp_machine_completion[machine]) ? temp_job_completion[j] : temp_machine_completion[machine];

            // Tempo para completar = tempo de inicio + duração
            temp_job_completion[j] = start_time + duration;
            // Atualiza o tempo de conclusão da máquina
            temp_machine_completion[machine] = start_time + duration;
        }
    }

    int makespan = 0;
    // Por cada job verifica o tempo de conclusão, caso seja maior que o makespan, atualiza o makespan
    for (int j = 0; j < num_jobs; j++)
    {
        if (temp_job_completion[j] > makespan)
            makespan = temp_job_completion[j];
    }

    // Retorna o makespan
    return makespan;
}

// Calcula um limite inferior para o makespan a partir do estado atual
int calculate_lower_bound(int job_completion[], int machine_completion[], int job_next_op[])
{
    int max_bound = 0;

    // Para cada job, calcula o tempo mínimo para completá-lo (tempo atual + operações restantes)
    for (int j = 0; j < num_jobs; j++)
    {
        int job_bound = job_completion[j] + job_remaining_time[j][job_next_op[j]];
        if (job_bound > max_bound)
            max_bound = job_bound;
    }

    // Para cada máquina, calcula o tempo mínimo para processar todas as operações restantes nela
    for (int m = 0; m < num_machines; m++)
    {
        int remaining_work = 0;
        int earliest_available = machine_completion[m];

        // Estrutura para armazenar operações restantes nesta máquina
        typedef struct
        {
            int job;
            int op;
            int earliest_start;
            int duration;
        } OpInfo;
        OpInfo ops[MAX_JOBS * MAX_MACHINES];
        int num_ops = 0;

        // Coleta todas as operações restantes que precisam desta máquina
        for (int j = 0; j < num_jobs; j++)
        {
            for (int op = job_next_op[j]; op < num_machines; op++)
            {
                if (job_machine[j][op] == m)
                {
                    ops[num_ops].job = j;
                    ops[num_ops].op = op;
                    ops[num_ops].duration = job_duration[j][op];

                    // Calcula o tempo mais cedo que esta operação pode começar (considerando dependências do job)
                    int job_ready_time = job_completion[j];
                    for (int prev_op = job_next_op[j]; prev_op < op; prev_op++)
                    {
                        job_ready_time += job_duration[j][prev_op];
                    }
                    ops[num_ops].earliest_start = job_ready_time;
                    num_ops++;
                }
            }
        }

        // Ordena as operações por tempo mais cedo de início (simula processamento sequencial na máquina)
        for (int i = 0; i < num_ops - 1; i++)
        {
            for (int k = i + 1; k < num_ops; k++)
            {
                if (ops[k].earliest_start < ops[i].earliest_start)
                {
                    OpInfo temp = ops[i];
                    ops[i] = ops[k];
                    ops[k] = temp;
                }
            }
        }

        // Simula o processamento das operações nesta máquina, respeitando os tempos de início
        int current_time = earliest_available;
        for (int i = 0; i < num_ops; i++)
        {
            if (ops[i].earliest_start > current_time)
            {
                current_time = ops[i].earliest_start;
            }
            current_time += ops[i].duration;
        }

        // Atualiza o limite inferior se necessário
        if (current_time > max_bound)
            max_bound = current_time;
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

// Main Branch and Bound function with aggressive improvements
void branch_and_bound(int schedule[MAX_JOBS][MAX_MACHINES],
                      int job_completion[],
                      int machine_completion[],
                      int job_next_op[],
                      int depth)
{
    // Node limit control
#ifdef _OPENMP
    static thread_local long long thread_nodes = 0;
    thread_nodes++;
    if (thread_nodes > MAX_NODES_PER_THREAD)
    {
        return;
    }
#else
    if (nodes_explored > MAX_TOTAL_NODES)
    {
        return;
    }
#endif

#ifdef _OPENMP
#pragma omp atomic
#endif
    nodes_explored++;

    // Progress indicator - show more frequently to track progress better
#ifdef _OPENMP
    if (omp_get_thread_num() == 0 && nodes_explored % 5000000 == 0)
    {
        double elapsed = getClock() - global_start_time;
        printf("Nos explorados: %lld, melhor makespan: %d, tempo: %.1fs\n",
               nodes_explored, best_makespan, elapsed);
    }
#else
    if (nodes_explored % 5000000 == 0)
    {
        double elapsed = getClock() - global_start_time;
        printf("Nos explorados: %lld, melhor makespan: %d, tempo: %.1fs\n",
               nodes_explored, best_makespan, elapsed);
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

        printf("Solucao completa encontrada: makespan = %d (nos: %lld)\n", makespan, nodes_explored);
        update_best_solution(schedule, makespan);
        return;
    }

    // Less aggressive depth control - allow much deeper exploration
    int max_reasonable_depth = num_jobs * num_machines; // Full depth allowed
    if (depth > max_reasonable_depth)
    {
        return;
    }

    // Improved lower bound pruning
    int lower_bound = calculate_lower_bound(job_completion, machine_completion, job_next_op);
    if (lower_bound >= best_makespan)
    {
        return;
    }

    // Enhanced job selection with priority scoring
    typedef struct
    {
        int job;
        int priority_score;
        int remaining_time;
        int duration;
        int earliest_start;
        int machine;
        int op;
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
            available_jobs[num_available].machine = machine;
            available_jobs[num_available].op = op;

            // Multi-criteria priority scoring
            int urgency = job_remaining_time[j][op]; // More remaining work = higher priority
            int bottleneck = 0;

            // Check if this operation uses a busy machine (bottleneck detection)
            for (int other_j = 0; other_j < num_jobs; other_j++)
            {
                for (int other_op = job_next_op[other_j]; other_op < num_machines; other_op++)
                {
                    if (job_machine[other_j][other_op] == machine)
                        bottleneck++;
                }
            }

            // Combine criteria: urgency (remaining work) + bottleneck factor + duration preference
            available_jobs[num_available].priority_score = urgency * 100 + bottleneck * 20 + duration * 5;
            num_available++;
        }
    }

    // Sort by priority score (higher = better)
    for (int i = 0; i < num_available - 1; i++)
    {
        for (int k = i + 1; k < num_available; k++)
        {
            if (available_jobs[k].priority_score > available_jobs[i].priority_score)
            {
                JobInfo temp = available_jobs[i];
                available_jobs[i] = available_jobs[k];
                available_jobs[k] = temp;
            }
        }
    }

    // Much less restrictive branching - allow full exploration at most depths
    int max_branches;
    if (depth < 15)
        max_branches = num_available; // Full branching for most depths
    else if (depth < 20)
        max_branches = (num_available > 3) ? 3 : num_available; // Slight limiting at very deep levels
    else
        max_branches = (num_available > 2) ? 2 : num_available; // Only limit at extremely deep levels

// Parallelize at moderate depths to balance efficiency with exploration
#ifdef _OPENMP
#pragma omp parallel for if (depth <= 3 && max_branches > 1) schedule(dynamic)
#endif
    for (int idx = 0; idx < max_branches; idx++)
    {
        int j = available_jobs[idx].job;
        int op = available_jobs[idx].op;
        int machine = available_jobs[idx].machine;
        int duration = available_jobs[idx].duration;
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
    global_start_time = getClock();

    read_input();

#ifdef _OPENMP
    omp_init_lock(&best_lock);
    printf("=== BALANCED PARALLEL BRANCH AND BOUND ===\n");
    printf("Threads disponiveis: %d\n", omp_get_max_threads());
    printf("Limite de nos por thread: %dM\n", MAX_NODES_PER_THREAD / 1000000);
#else
    printf("=== BALANCED BRANCH AND BOUND PARA JOB SHOP ===\n");
    printf("Limite total de nos: %dM\n", MAX_TOTAL_NODES / 1000000);
#endif

    // Get better initial upper bound
    best_makespan = get_initial_upper_bound();
    printf("Upper bound inicial (heuristica): %d\n", best_makespan);

    // Save the heuristic solution as initial best
    int temp_job_completion[MAX_JOBS] = {0};
    int temp_machine_completion[MAX_MACHINES] = {0};

    for (int j = 0; j < num_jobs; j++)
    {
        for (int op = 0; op < num_machines; op++)
        {
            int machine = job_machine[j][op];
            int duration = job_duration[j][op];
            int start_time = (temp_job_completion[j] > temp_machine_completion[machine]) ? temp_job_completion[j] : temp_machine_completion[machine];

            best_schedule[j][op] = start_time; // Save the heuristic schedule
            temp_job_completion[j] = start_time + duration;
            temp_machine_completion[machine] = start_time + duration;
        }
    }

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
            // Don't initialize best_schedule to -1, let the heuristic set it
        }
    }
    for (int m = 0; m < num_machines; m++)
    {
        machine_completion[m] = 0;
    }

    printf("Iniciando Optimized Branch and Bound...\n");
    printf("Heuristica salva como solucao inicial.\n");

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
            // Show all operations, even if start time might be 0
            printf("Op%d(M%d,t=%d->%d) ", op, job_machine[j][op],
                   best_schedule[j][op], best_schedule[j][op] + job_duration[j][op]);
        }
        printf("\n");
    }

    return 0;
}