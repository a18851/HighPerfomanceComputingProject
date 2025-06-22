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
#define MAX_TOTAL_NODES 10000000000

int num_jobs, num_machines;
int job_machine[MAX_JOBS][MAX_MACHINES];
int job_duration[MAX_JOBS][MAX_MACHINES];

int best_makespan;
int best_schedule[MAX_JOBS][MAX_MACHINES];
long long nodes_explored = 0;

int job_remaining_time[MAX_JOBS][MAX_MACHINES + 1];

double global_start_time = 0;

#ifdef _OPENMP
omp_lock_t best_lock;
#endif

void read_input(const char *input_filename)
{
    // Abre o ficheiro de entrada para leitura
    FILE *input = fopen(input_filename, "r");
    if (!input)
    {
        // Se não conseguir abrir o ficheiro, exibe mensagem de erro e encerra o programa
        printf("ERRO: Ficheiro %s nao encontrado\n", input_filename);
        exit(1);
    }

    // Lê o número de jobs e de máquinas do ficheiro
    fscanf(input, "%d %d", &num_jobs, &num_machines);
    printf("Problema: %d jobs, %d machines\n", num_jobs, num_machines);

    // Lê, para cada job, a sequência de máquinas e suas durações
    for (int j = 0; j < num_jobs; j++)
    {
        for (int op = 0; op < num_machines; op++)
        {
            fscanf(input, "%d %d", &job_machine[j][op], &job_duration[j][op]);
        }
    }
    fclose(input); // Fecha o ficheiro após a leitura

    // Calcula o tempo restante de processamento para cada operação de cada job
    for (int j = 0; j < num_jobs; j++)
    {
        job_remaining_time[j][num_machines] = 0; // Inicializa o tempo restante após a última operação como zero
        for (int op = num_machines - 1; op >= 0; op--)
        {
            // Soma a duração da operação atual ao tempo restante das operações seguintes
            job_remaining_time[j][op] = job_remaining_time[j][op + 1] + job_duration[j][op];
        }
    }

    // Exibe os dados lidos do problema para conferência
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

int get_initial_upper_bound()
{
    // Vetores temporários para armazenar o tempo de conclusão de cada job e máquina
    int temp_job_completion[MAX_JOBS] = {0};
    int temp_machine_completion[MAX_MACHINES] = {0};

    // Para cada job
    for (int j = 0; j < num_jobs; j++)
    {
        // Para cada operação do job
        for (int op = 0; op < num_machines; op++)
        {
            int machine = job_machine[j][op];   // Máquina da operação atual
            int duration = job_duration[j][op]; // Duração da operação atual

            // O início da operação é o maior valor entre o término do job e o término da máquina
            int start_time = (temp_job_completion[j] > temp_machine_completion[machine]) ? temp_job_completion[j] : temp_machine_completion[machine];

            // Atualiza o tempo de conclusão do job e da máquina
            temp_job_completion[j] = start_time + duration;
            temp_machine_completion[machine] = start_time + duration;
        }
    }

    // Calcula o makespan (tempo total para concluir todos os jobs)
    int makespan = 0;
    for (int j = 0; j < num_jobs; j++)
    {
        if (temp_job_completion[j] > makespan)
            makespan = temp_job_completion[j];
    }

    return makespan; // Retorna o makespan como upper bound inicial
}

int is_dominated_state(int job_completion[], int machine_completion[], int job_next_op[])
{
    // Verifica se algum estado é dominado, ou seja, se algum job está muito atrasado em relação a outro com a mesma próxima operação
    for (int j1 = 0; j1 < num_jobs; j1++)
    {
        for (int j2 = j1 + 1; j2 < num_jobs; j2++)
        {
            if (job_next_op[j1] == job_next_op[j2])
            {
                // Define um limiar de atraso baseado no melhor makespan conhecido
                int delay_threshold = (best_makespan * 2) / 3;
                // Se o job j1 está muito mais atrasado que j2, o estado é dominado
                if (job_completion[j1] > job_completion[j2] + delay_threshold)
                {
                    return 1;
                }
            }
        }
    }

    return 0; // Estado não dominado
}

int calculate_improved_lower_bound(int job_completion[], int machine_completion[], int job_next_op[])
{
    int max_bound = 0;

    // Calcula o bound baseado no tempo restante de cada job
    for (int j = 0; j < num_jobs; j++)
    {
        int job_bound = job_completion[j] + job_remaining_time[j][job_next_op[j]];
        if (job_bound > max_bound)
            max_bound = job_bound;
    }

    // Para cada máquina, calcula o bound considerando as operações restantes nela
    for (int m = 0; m < num_machines; m++)
    {
        int remaining_work = 0;
        int earliest_available = machine_completion[m];

        // Estrutura para armazenar informações das operações restantes na máquina m
        typedef struct
        {
            int job;
            int op;
            int earliest_start;
            int duration;
        } OpInfo;
        OpInfo ops[MAX_JOBS * MAX_MACHINES];
        int num_ops = 0;

        // Coleta todas as operações restantes para a máquina m
        for (int j = 0; j < num_jobs; j++)
        {
            for (int op = job_next_op[j]; op < num_machines; op++)
            {
                if (job_machine[j][op] == m)
                {
                    ops[num_ops].job = j;
                    ops[num_ops].op = op;
                    ops[num_ops].duration = job_duration[j][op];

                    // Calcula o tempo mais cedo que a operação pode começar
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

        // Ordena as operações pelo tempo mais cedo de início (selection sort)
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

        // Simula o processamento das operações na máquina m
        int current_time = earliest_available;
        for (int i = 0; i < num_ops; i++)
        {
            if (ops[i].earliest_start > current_time)
            {
                current_time = ops[i].earliest_start;
            }
            current_time += ops[i].duration;
        }

        // Atualiza o bound máximo se necessário
        if (current_time > max_bound)
            max_bound = current_time;
    }

    return max_bound; // Retorna o melhor lower bound encontrado
}

int all_jobs_complete(int job_next_op[])
{
    // Verifica se todos os jobs já completaram todas as operações
    for (int j = 0; j < num_jobs; j++)
    {
        if (job_next_op[j] < num_machines)
            return 0; // Ainda há operações a serem feitas
    }
    return 1; // Todos os jobs estão completos
}

void update_best_solution(int schedule[MAX_JOBS][MAX_MACHINES], int makespan)
{
#ifdef _OPENMP
    // Se estiver usando OpenMP, trava o acesso à melhor solução para evitar condições de corrida
    omp_set_lock(&best_lock);
#endif

    // Verifica se o novo makespan é melhor (menor) que o melhor encontrado até agora
    if (makespan < best_makespan)
    {
        best_makespan = makespan; // Atualiza o melhor makespan
        // Copia o agendamento atual para o melhor agendamento encontrado
        for (int j = 0; j < num_jobs; j++)
        {
            for (int op = 0; op < num_machines; op++)
            {
                best_schedule[j][op] = schedule[j][op];
            }
        }

#ifdef _OPENMP
        // Imprime mensagem informando a nova melhor solução, incluindo o número da thread
        printf("Nova melhor solucao: makespan = %d (thread %d, nos: %lld)\n",
               best_makespan, omp_get_thread_num(), nodes_explored);
#else
        // Imprime mensagem informando a nova melhor solução (versão sequencial)
        printf("Nova melhor solucao: makespan = %d (nos: %lld)\n",
               best_makespan, nodes_explored);
#endif
    }

#ifdef _OPENMP
    // Libera o lock após atualizar a melhor solução
    omp_unset_lock(&best_lock);
#endif
}

void branch_and_bound(int schedule[MAX_JOBS][MAX_MACHINES],
                      int job_completion[],
                      int machine_completion[],
                      int job_next_op[],
                      int depth)
{
    // Limita o número total de nós explorados para evitar execuções muito longas
    if (nodes_explored >= MAX_TOTAL_NODES)
    {
        return;
    }

    // Incrementa o contador de nós explorados de forma atômica em ambiente paralelo
#ifdef _OPENMP
#pragma omp atomic
#endif
    nodes_explored++;

    // Periodicamente, imprime estatísticas de progresso (apenas pela thread 0 em paralelo)
#ifdef _OPENMP
    if (omp_get_thread_num() == 0 && nodes_explored % 5000000 == 0)
    {
        double elapsed = getClock() - global_start_time;
        printf("Nos explorados: %lld/%lld, melhor makespan: %d, tempo: %.1fs\n",
               nodes_explored, (long long)MAX_TOTAL_NODES, best_makespan, elapsed);
    }
#else
    if (nodes_explored % 5000000 == 0)
    {
        double elapsed = getClock() - global_start_time;
        printf("Nos explorados: %lld/%lld, melhor makespan: %d, tempo: %.1fs\n",
               nodes_explored, (long long)MAX_TOTAL_NODES, best_makespan, elapsed);
    }
#endif

    // Se todos os jobs estão completos, verifica e atualiza a melhor solução
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

    // Limita a profundidade máxima da busca para evitar loops infinitos
    int max_reasonable_depth = num_jobs * num_machines;
    if (depth > max_reasonable_depth)
    {
        return;
    }

    // Calcula um lower bound para o makespan a partir do estado atual
    int lower_bound = calculate_improved_lower_bound(job_completion, machine_completion, job_next_op);
    if (lower_bound >= best_makespan)
    {
        // Poda: não vale a pena explorar este ramo
        return;
    }

    // Estrutura para armazenar informações dos jobs disponíveis para ramificação
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

    // Identifica todos os jobs que ainda têm operações a serem agendadas
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

            // Calcula uma prioridade para o job com base em urgência, gargalo e duração
            int urgency = job_remaining_time[j][op];
            int bottleneck = 0;

            // Conta quantas operações restantes usam a mesma máquina (gargalo)
            for (int other_j = 0; other_j < num_jobs; other_j++)
            {
                for (int other_op = job_next_op[other_j]; other_op < num_machines; other_op++)
                {
                    if (job_machine[other_j][other_op] == machine)
                        bottleneck++;
                }
            }

            available_jobs[num_available].priority_score = urgency * 100 + bottleneck * 20 + duration * 5;
            num_available++;
        }
    }

    // Ordena os jobs disponíveis por prioridade (maior score primeiro)
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

    // Define o número máximo de ramos a explorar, reduzindo conforme a profundidade aumenta
    int max_branches;
    if (depth < 15)
        max_branches = num_available;
    else if (depth < 20)
        max_branches = (num_available > 3) ? 3 : num_available;
    else
        max_branches = (num_available > 2) ? 2 : num_available;

    // Paraleliza a ramificação nos primeiros níveis da árvore de busca
#ifdef _OPENMP
#pragma omp parallel for if (depth <= 6 && max_branches > 1) schedule(dynamic)
#endif
    for (int i = 0; i < max_branches; i++)
    {
        // Garante que não ultrapasse o limite de nós explorados
        if (nodes_explored >= MAX_TOTAL_NODES)
        {
            continue;
        }

        int j = available_jobs[i].job;
        int op = available_jobs[i].op;
        int machine = available_jobs[i].machine;
        int duration = available_jobs[i].duration;
        int start_time = available_jobs[i].earliest_start;
        int end_time = start_time + duration;

        // Cria cópias dos vetores para o novo estado
        int new_schedule[MAX_JOBS][MAX_MACHINES];
        int new_job_completion[MAX_JOBS];
        int new_machine_completion[MAX_MACHINES];
        int new_job_next_op[MAX_JOBS];

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

        // Atualiza o novo estado com a operação escolhida
        new_schedule[j][op] = start_time;
        new_job_completion[j] = end_time;
        new_machine_completion[machine] = end_time;
        new_job_next_op[j]++;

        // Chama recursivamente para o novo estado
        branch_and_bound(new_schedule, new_job_completion, new_machine_completion,
                         new_job_next_op, depth + 1);
    }
}

int main(int argc, char **argv)
{
    // Verifica se o número de argumentos está correto
    if (argc != 4)
    {
        printf("Uso: %s <input_file> <output_file> <metrics_file>\n", argv[0]);
        printf("Exemplo: %s input/05.jss output/bnb_par.txt output/bnb_par_metrics.txt\n", argv[0]);
        return 1;
    }

    // Lê os nomes dos ficheiros a partir dos argumentos
    const char *input_filename = argv[1];
    const char *output_filename = argv[2];
    const char *metrics_filename = argv[3];

    // Inicializa variáveis globais de controle
    nodes_explored = 0;
    global_start_time = getClock();

    // Lê os dados do problema do ficheiro de entrada
    read_input(input_filename);

#ifdef _OPENMP
    // Inicializa o lock para acesso concorrente à melhor solução
    omp_init_lock(&best_lock);
    printf("=== BALANCED PARALLEL BRANCH AND BOUND (FIXED NODE LIMIT) ===\n");
    printf("Threads disponiveis: %d\n", omp_get_max_threads());
    printf("Limite TOTAL de nos: %dM (fixo, nao por thread)\n", MAX_TOTAL_NODES / 1000000);
#else
    printf("=== BALANCED BRANCH AND BOUND PARA JOB SHOP ===\n");
    printf("Limite total de nos: %dM\n", MAX_TOTAL_NODES / 1000000);
#endif
    printf("Ficheiro de entrada: %s\n", input_filename);
    printf("Ficheiro de saida: %s\n", output_filename);
    printf("Ficheiro de metricas: %s\n\n", metrics_filename);

    // Calcula o upper bound inicial usando uma heurística
    best_makespan = get_initial_upper_bound();
    printf("Upper bound inicial (heuristica): %d\n", best_makespan);

    // Gera o escalonamento heurístico inicial e guarda na variavel best_schedule
    int temp_job_completion[MAX_JOBS] = {0};
    int temp_machine_completion[MAX_MACHINES] = {0};
    for (int j = 0; j < num_jobs; j++)
    {
        for (int op = 0; op < num_machines; op++)
        {
            int machine = job_machine[j][op];
            int duration = job_duration[j][op];
            int start_time = (temp_job_completion[j] > temp_machine_completion[machine]) ? temp_job_completion[j] : temp_machine_completion[machine];

            best_schedule[j][op] = start_time;
            temp_job_completion[j] = start_time + duration;
            temp_machine_completion[machine] = start_time + duration;
        }
    }

    // Inicializa as estruturas para o algoritmo Branch and Bound
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
        }
    }
    for (int m = 0; m < num_machines; m++)
    {
        machine_completion[m] = 0;
    }

    printf("Iniciando Optimized Branch and Bound...\n");
    printf("Heuristica guardada como solucao inicial.\n");

    // Marca o tempo de início da execução do Branch and Bound (CPU e wall clock)
    clock_t start_time = clock();
    double wall_start = getClock();

    // Executa o algoritmo Branch and Bound
    branch_and_bound(schedule, job_completion, machine_completion, job_next_op, 0);

    // Marca o tempo de término
    clock_t end_time = clock();
    double wall_end = getClock();
    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    double wall_elapsed = wall_end - wall_start;

#ifdef _OPENMP
    // Destroi o lock após o uso
    omp_destroy_lock(&best_lock);
#endif

    // Guarda o melhor escalonamento encontrado no ficheiro de saída
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
        printf("Erro ao criar ficheiro de saida: %s\n", output_filename);
    }

    // Guarda as métricas de execução no ficheiro de métricas
    FILE *metrics = fopen(metrics_filename, "w");
    if (metrics)
    {
        fprintf(metrics, "Tempo de execucao (CPU): %.4f segundos\n", elapsed);
        fprintf(metrics, "Tempo de execucao (Wall): %.4f segundos\n", wall_elapsed);
        fprintf(metrics, "Makespan: %d\n", best_makespan);
        fprintf(metrics, "Nos explorados: %lld\n", nodes_explored);
        fprintf(metrics, "Limite total de nos: %d\n", MAX_TOTAL_NODES);
        fprintf(metrics, "Ficheiro de entrada: %s\n", input_filename);
#ifdef _OPENMP
        fprintf(metrics, "Algoritmo: Branch and Bound Paralelo (Limite Fixo)\n");
        fprintf(metrics, "Threads utilizadas: %d\n", omp_get_max_threads());
        fprintf(metrics, "Speedup: %.2fx\n", elapsed > 0 ? elapsed / wall_elapsed : 1.0);
#else
        fprintf(metrics, "Algoritmo: Branch and Bound Sequencial\n");
#endif
        fclose(metrics);
    }
    else
    {
        printf("Erro ao criar Ficheiro de metricas: %s\n", metrics_filename);
    }

    // Exibe os resultados finais no terminal
    printf("\n=== RESULTADOS ===\n");
    printf("Melhor makespan: %d\n", best_makespan);
    printf("Tempo de execucao: %.4f segundos\n", wall_elapsed);
    printf("Nos explorados: %lld / %d (%.1f%%)\n",
           nodes_explored, MAX_TOTAL_NODES,
           (double)nodes_explored / MAX_TOTAL_NODES * 100.0);
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
            printf("Op%d(M%d,t=%d->%d) ", op, job_machine[j][op],
                   best_schedule[j][op], best_schedule[j][op] + job_duration[j][op]);
        }
        printf("\n");
    }

    printf("\nResultados guardados em: %s\n", output_filename);
    printf("Metricas guardadas em: %s\n", metrics_filename);

    return 0;
}