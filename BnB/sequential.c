#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#define MAX_JOBS 8
#define MAX_MACHINES 8
#define MAX_TOTAL_NODES 10000000000 // 1000M total nodes limit

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

void read_input(const char *input_filename)
{
    FILE *input = fopen(input_filename, "r");
    if (!input)
    {
        printf("ERRO: Arquivo %s nao encontrado\n", input_filename);
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

    // For cada job
    for (int j = 0; j < num_jobs; j++)
    {
        // For cada operação dentro do job
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

// Dominance-based pruning function (very conservative)
int is_dominated_state(int job_completion[], int machine_completion[], int job_next_op[])
{
    // Only apply very conservative dominance pruning to avoid eliminating good paths
    for (int j1 = 0; j1 < num_jobs; j1++)
    {
        for (int j2 = j1 + 1; j2 < num_jobs; j2++)
        {
            if (job_next_op[j1] == job_next_op[j2])
            {
                // Only prune if delay is very significant (more than 2/3 of best makespan)
                int delay_threshold = (best_makespan * 2) / 3;
                if (job_completion[j1] > job_completion[j2] + delay_threshold)
                {
                    return 1;
                }
            }
        }
    }

    return 0;
}

// Improved lower bound calculation (conservative)
int calculate_improved_lower_bound(int job_completion[], int machine_completion[], int job_next_op[])
{
    int max_bound = 0;

    // Original bounds
    for (int j = 0; j < num_jobs; j++)
    {
        int job_bound = job_completion[j] + job_remaining_time[j][job_next_op[j]];
        if (job_bound > max_bound)
            max_bound = job_bound;
    }

    // Enhanced machine-based bound with better estimation
    for (int m = 0; m < num_machines; m++)
    {
        int remaining_work = 0;
        int earliest_available = machine_completion[m];

        // Collect all remaining operations for this machine
        typedef struct
        {
            int job;
            int op;
            int earliest_start;
            int duration;
        } OpInfo;
        OpInfo ops[MAX_JOBS * MAX_MACHINES];
        int num_ops = 0;

        for (int j = 0; j < num_jobs; j++)
        {
            for (int op = job_next_op[j]; op < num_machines; op++)
            {
                if (job_machine[j][op] == m)
                {
                    ops[num_ops].job = j;
                    ops[num_ops].op = op;
                    ops[num_ops].duration = job_duration[j][op];

                    // Calculate earliest this operation can start
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

        // Sort by earliest start time for tighter bound
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

        // Calculate machine bound considering operation ordering
        int current_time = earliest_available;
        for (int i = 0; i < num_ops; i++)
        {
            if (ops[i].earliest_start > current_time)
            {
                current_time = ops[i].earliest_start;
            }
            current_time += ops[i].duration;
        }

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

void update_best_solution(int schedule[MAX_JOBS][MAX_MACHINES], int makespan)
{
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
        printf("Nova melhor solucao: makespan = %d (nos explorados: %lld)\n",
               best_makespan, nodes_explored);
    }
}

// Enhanced branching with improved operation ordering and limits
void branch_and_bound(int schedule[MAX_JOBS][MAX_MACHINES],
                      int job_completion[],
                      int machine_completion[],
                      int job_next_op[],
                      int depth)
{
    // Node limit control
    if (nodes_explored > MAX_TOTAL_NODES)
    {
        return;
    }

    // Incrementa o contador de nós explorados
    nodes_explored++;

    // Printa o progresso a cada 5 milhões de nós explorados
    if (nodes_explored % 5000000 == 0)
    {
        double elapsed = ((double)clock() / CLOCKS_PER_SEC);
        printf("Nos explorados: %lld, melhor makespan: %d, tempo: %.1fs\n",
               nodes_explored, best_makespan, elapsed);
    }

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

    // Allow full depth exploration
    int max_reasonable_depth = num_jobs * num_machines;
    if (depth > max_reasonable_depth)
    {
        return;
    }

    // Conservative dominance-based pruning (optional - can be disabled)
    // if (is_dominated_state(job_completion, machine_completion, job_next_op))
    // {
    //     return;
    // }

    // Improved lower bound pruning
    int lower_bound = calculate_improved_lower_bound(job_completion, machine_completion, job_next_op);
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

    // Limit branching factor based on depth to control explosion
    int max_branches;
    if (depth < 15)
        max_branches = num_available; // Full branching for most depths
    else if (depth < 20)
        max_branches = (num_available > 3) ? 3 : num_available; // Slight limiting at very deep levels
    else
        max_branches = (num_available > 2) ? 2 : num_available; // Only limit at extremely deep levels

    // Branch on jobs in order
    for (int i = 0; i < max_branches; i++)
    {
        int j = available_jobs[i].job;
        int op = available_jobs[i].op;
        int machine = available_jobs[i].machine;
        int duration = available_jobs[i].duration;
        int start_time = available_jobs[i].earliest_start;
        int end_time = start_time + duration;

        // Create new state
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

int main(int argc, char **argv)
{
    // Check command line arguments
    if (argc != 4)
    {
        printf("Uso: %s <input_file> <output_file> <metrics_file>\n", argv[0]);
        printf("Exemplo: %s input/05.jss output/bnb_seq.txt output/bnb_seq_metrics.txt\n", argv[0]);
        return 1;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];
    const char *metrics_filename = argv[3];

    printf("=== OPTIMIZED SEQUENTIAL BRANCH AND BOUND ===\n");
    printf("Limite total de nos: %lldM\n", (long long)(MAX_TOTAL_NODES / 1000000));
    printf("Arquivo de entrada: %s\n", input_filename);
    printf("Arquivo de saida: %s\n", output_filename);
    printf("Arquivo de metricas: %s\n\n", metrics_filename);

    nodes_explored = 0;
    global_start_time = (double)clock() / CLOCKS_PER_SEC;

    read_input(input_filename);

    // Obtem a melhor solução inicial
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
    // Inicializa os arrays de escalonamento, conclusão dos jobs e máquinas, e próximo operação de cada job
    int schedule[MAX_JOBS][MAX_MACHINES];
    int job_completion[MAX_JOBS];
    int machine_completion[MAX_MACHINES];
    int job_next_op[MAX_JOBS];

    // Preenche os arrays com valores iniciais
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
    // Inicia a busca Branch and Bound
    branch_and_bound(schedule, job_completion, machine_completion, job_next_op, 0);
    clock_t end_time = clock();

    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    // Write results
    FILE *output = fopen(output_filename, "w");
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
    else
    {
        printf("Erro ao criar arquivo de saida: %s\n", output_filename);
    }

    FILE *metrics = fopen(metrics_filename, "w");
    if (metrics)
    {
        fprintf(metrics, "Tempo de execucao: %.4f segundos\n", elapsed);
        fprintf(metrics, "Makespan: %d\n", best_makespan);
        fprintf(metrics, "Nos explorados: %lld\n", nodes_explored);
        fprintf(metrics, "Algoritmo: Branch and Bound Sequencial\n");
        fprintf(metrics, "Arquivo de entrada: %s\n", input_filename);
        fclose(metrics);
    }
    else
    {
        printf("Erro ao criar arquivo de metricas: %s\n", metrics_filename);
    }

    printf("\n=== RESULTADOS ===\n");
    printf("Melhor makespan: %d\n", best_makespan);
    printf("Tempo de execucao: %.4f segundos\n", elapsed);
    printf("Nos explorados: %lld\n", nodes_explored);

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

    printf("\nResultados salvos em: %s\n", output_filename);
    printf("Metricas salvas em: %s\n", metrics_filename);

    return 0;
}