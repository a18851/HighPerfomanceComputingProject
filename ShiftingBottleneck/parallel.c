#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

// Se OpenMP estiver disponível, inclui e define funções para paralelismo
#ifdef _OPENMP
#include <omp.h>
#define getClock() omp_get_wtime()
#else
#include <time.h>
#define getClock() ((double)clock() / CLOCKS_PER_SEC)
#endif

#define MAX_JOBS 105     // Número máximo de jobs
#define MAX_MACHINES 105 // Número máximo de máquinas

// Variáveis globais para armazenar dados do problema
int num_jobs, num_machines;
int job_machine[MAX_JOBS][MAX_MACHINES];  // Máquina de cada operação de cada job
int job_duration[MAX_JOBS][MAX_MACHINES]; // Duração de cada operação de cada job

int best_makespan;                         // Melhor makespan encontrado
int best_schedule[MAX_JOBS][MAX_MACHINES]; // Melhor escalonamento encontrado

// Estrutura para representar uma operação
typedef struct
{
    int job;
    int operation;
    int start_time;
    int end_time;
    int machine;
    int duration;
} Operation;

Operation machine_schedule[MAX_MACHINES][MAX_JOBS * MAX_MACHINES]; // Escalonamento por máquina
int machine_op_count[MAX_MACHINES];                                // Número de operações por máquina

int job_completion_time[MAX_JOBS];                // Tempo de conclusão de cada job
int machine_completion_time[MAX_MACHINES];        // Tempo de conclusão de cada máquina
int operation_start_time[MAX_JOBS][MAX_MACHINES]; // Tempo de início de cada operação

#ifdef _OPENMP
omp_lock_t schedule_lock; // Lock para sincronização em OpenMP
#endif

// Função para ler o ficheiro de input
void read_input(const char *input_filename)
{
    FILE *input = fopen(input_filename, "r");
    if (!input)
    {
        printf("ERRO: Ficheiro %s nao encontrado\n", input_filename);
        exit(1);
    }

    fscanf(input, "%d %d", &num_jobs, &num_machines);
    printf("Problema: %d jobs, %d machines\n", num_jobs, num_machines);

    // Leitura das operações de cada job
    for (int j = 0; j < num_jobs; j++)
    {
        for (int op = 0; op < num_machines; op++)
        {
            fscanf(input, "%d %d", &job_machine[j][op], &job_duration[j][op]);
        }
    }
    fclose(input);

    // Impressão dos dados lidos
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

// Inicializa as estruturas de dados para uma nova solução
void initialize_solution()
{
    for (int j = 0; j < num_jobs; j++)
    {
        job_completion_time[j] = 0;
        for (int op = 0; op < num_machines; op++)
        {
            operation_start_time[j][op] = 0;
            best_schedule[j][op] = 0;
        }
    }

    for (int m = 0; m < num_machines; m++)
    {
        machine_completion_time[m] = 0;
        machine_op_count[m] = 0;
    }

    best_makespan = INT_MAX;

#ifdef _OPENMP
    omp_init_lock(&schedule_lock);
#endif
}

// Calcula os tempos mais cedo possíveis de início das operações
void calculate_earliest_start_times()
{
    for (int j = 0; j < num_jobs; j++)
    {
        job_completion_time[j] = 0;
        for (int op = 0; op < num_machines; op++)
        {
            operation_start_time[j][op] = 0;
        }
    }

    for (int m = 0; m < num_machines; m++)
    {
        machine_completion_time[m] = 0;
    }

    // Para cada job, calcula o início de cada operação sequencialmente
    for (int j = 0; j < num_jobs; j++)
    {
        int current_time = 0;
        for (int op = 0; op < num_machines; op++)
        {
            operation_start_time[j][op] = current_time;
            current_time += job_duration[j][op];
            job_completion_time[j] = current_time;
        }
    }
}

// Calcula a carga de trabalho total de uma máquina
int calculate_machine_workload(int machine)
{
    int total_workload = 0;

#ifdef _OPENMP
#pragma omp parallel for reduction(+ : total_workload)
#endif
    for (int j = 0; j < num_jobs; j++)
    {
        for (int op = 0; op < num_machines; op++)
        {
            if (job_machine[j][op] == machine)
            {
                total_workload += job_duration[j][op];
            }
        }
    }
    return total_workload;
}

// Escalona as operações de uma máquina
void schedule_machine_operations(int machine)
{
    Operation operations[MAX_JOBS * MAX_MACHINES];
    int op_count = 0;

    // Seleciona operações que pertencem à máquina
    for (int j = 0; j < num_jobs; j++)
    {
        for (int op = 0; op < num_machines; op++)
        {
            if (job_machine[j][op] == machine)
            {
                operations[op_count].job = j;
                operations[op_count].operation = op;
                operations[op_count].machine = machine;
                operations[op_count].duration = job_duration[j][op];
                operations[op_count].start_time = operation_start_time[j][op];
                op_count++;
            }
        }
    }

    // Ordena operações por tempo de início e duração
    for (int i = 0; i < op_count - 1; i++)
    {
        for (int k = i + 1; k < op_count; k++)
        {
            int swap = 0;

            if (operations[k].start_time < operations[i].start_time)
            {
                swap = 1;
            }
            else if (operations[k].start_time == operations[i].start_time &&
                     operations[k].duration < operations[i].duration)
            {
                swap = 1;
            }

            if (swap)
            {
                Operation temp = operations[i];
                operations[i] = operations[k];
                operations[k] = temp;
            }
        }
    }

    int current_machine_time = 0;
    machine_op_count[machine] = op_count;

    // Agenda as operações na máquina
    for (int i = 0; i < op_count; i++)
    {
        int earliest_start = operation_start_time[operations[i].job][operations[i].operation];
        int actual_start = (current_machine_time > earliest_start) ? current_machine_time : earliest_start;

        operations[i].start_time = actual_start;
        operations[i].end_time = actual_start + operations[i].duration;

        machine_schedule[machine][i] = operations[i];
        current_machine_time = operations[i].end_time;

#ifdef _OPENMP
        omp_set_lock(&schedule_lock);
#endif
        best_schedule[operations[i].job][operations[i].operation] = actual_start;
#ifdef _OPENMP
        omp_unset_lock(&schedule_lock);
#endif
    }

    machine_completion_time[machine] = current_machine_time;
}

// Atualiza o tempo de conclusão de cada job
void update_job_completion_times()
{
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (int j = 0; j < num_jobs; j++)
    {
        int completion = 0;
        for (int op = 0; op < num_machines; op++)
        {
            int start = best_schedule[j][op];
            int end = start + job_duration[j][op];
            if (end > completion)
                completion = end;
        }
        job_completion_time[j] = completion;
    }
}

// Calcula o makespan atual
int calculate_makespan()
{
    int makespan = 0;

#ifdef _OPENMP
#pragma omp parallel for reduction(max : makespan)
#endif
    for (int j = 0; j < num_jobs; j++)
    {
        if (job_completion_time[j] > makespan)
            makespan = job_completion_time[j];
    }
    return makespan;
}

// Tenta melhorar o escalonamento de uma máquina
int try_improve_machine_schedule(int machine)
{
    int saved_schedule[MAX_JOBS][MAX_MACHINES];
    for (int j = 0; j < num_jobs; j++)
    {
        for (int op = 0; op < num_machines; op++)
        {
            saved_schedule[j][op] = best_schedule[j][op];
        }
    }

    int saved_makespan = best_makespan;

#ifdef _OPENMP
#pragma omp critical
#endif
    {
        printf("Tentando melhorar escalonamento da maquina %d (thread %d)...\n",
               machine,
#ifdef _OPENMP
               omp_get_thread_num()
#else
               0
#endif
        );
    }

    calculate_earliest_start_times();
    schedule_machine_operations(machine);

    // Reescalona as outras máquinas
    for (int m = 0; m < num_machines; m++)
    {
        if (m != machine)
        {
            schedule_machine_operations(m);
        }
    }

    update_job_completion_times();
    int new_makespan = calculate_makespan();

    if (new_makespan < best_makespan)
    {
        best_makespan = new_makespan;
#ifdef _OPENMP
#pragma omp critical
#endif
        {
            printf("Melhoria encontrada na maquina %d! Novo makespan: %d\n", machine, best_makespan);
        }
        return 1;
    }
    else
    {
        // Restaura o escalonamento anterior
        for (int j = 0; j < num_jobs; j++)
        {
            for (int op = 0; op < num_machines; op++)
            {
                best_schedule[j][op] = saved_schedule[j][op];
            }
        }
        best_makespan = saved_makespan;
#ifdef _OPENMP
#pragma omp critical
#endif
        {
            printf("Nenhuma melhoria encontrada para maquina %d\n", machine);
        }
        return 0;
    }
}

// Algoritmo principal Shifting Bottleneck
void shifting_bottleneck_algorithm()
{
#ifdef _OPENMP
    printf("=== ALGORITMO SHIFTING BOTTLENECK PARALELO (OpenMP) ===\n");
    printf("Threads disponveis: %d\n\n", omp_get_max_threads());
#else
    printf("=== ALGORITMO SHIFTING BOTTLENECK SEQUENCIAL ===\n\n");
#endif

    initialize_solution();

    printf("Fase 1: Construindo escalonamento inicial...\n");
    calculate_earliest_start_times();

    int machine_order[MAX_MACHINES];
    int machine_workload[MAX_MACHINES];

#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (int m = 0; m < num_machines; m++)
    {
        machine_order[m] = m;
        machine_workload[m] = calculate_machine_workload(m);
    }

    // Ordena máquinas por carga de trabalho (decrescente)
    for (int i = 0; i < num_machines - 1; i++)
    {
        for (int j = i + 1; j < num_machines; j++)
        {
            if (machine_workload[machine_order[j]] > machine_workload[machine_order[i]])
            {
                int temp = machine_order[i];
                machine_order[i] = machine_order[j];
                machine_order[j] = temp;
            }
        }
    }

    printf("Ordem de escalonamento das maquinas por carga de trabalho:\n");
    for (int i = 0; i < num_machines; i++)
    {
        printf("Maquina %d (carga: %d)\n", machine_order[i], machine_workload[machine_order[i]]);
    }

    // Escalona cada máquina pela ordem definida
    for (int i = 0; i < num_machines; i++)
    {
        int machine = machine_order[i];
        printf("\nEscalonando maquina %d...\n", machine);
        schedule_machine_operations(machine);
        update_job_completion_times();
        calculate_earliest_start_times();
    }

    update_job_completion_times();
    best_makespan = calculate_makespan();
    printf("\nMakespan inicial: %d\n", best_makespan);

    printf("\nFase 2: Melhorando escalonamento (paralelo)...\n");
    int iteration = 0;
    int improved = 1;

    // Loop de melhoria até não haver melhorias ou atingir limite de iterações
    while (improved && iteration < 10)
    {
        improved = 0;
        iteration++;
        printf("\nIteracao %d de melhoria:\n", iteration);

        int local_improvements[MAX_MACHINES] = {0};

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
        for (int m = 0; m < num_machines; m++)
        {
            local_improvements[m] = try_improve_machine_schedule(m);
        }

        for (int m = 0; m < num_machines; m++)
        {
            if (local_improvements[m])
            {
                improved = 1;
            }
        }

        if (!improved)
        {
            printf("Nenhuma melhoria encontrada nesta iteracao.\n");
        }
    }

#ifdef _OPENMP
    printf("\nAlgoritmo Shifting Bottleneck Paralelo concluido.\n");
#else
    printf("\nAlgoritmo Shifting Bottleneck Sequencial concluido.\n");
#endif
    printf("Makespan final: %d\n", best_makespan);
    printf("Iteracoes de melhoria: %d\n", iteration);
}

// Função principal
int main(int argc, char **argv)
{
    if (argc != 4)
    {
        printf("Uso: %s <input_file> <output_file> <metrics_file>\n", argv[0]);
        printf("Exemplo: %s input/04.jss output/result.txt output/metrics.txt\n", argv[0]);
        return 1;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];
    const char *metrics_filename = argv[3];

    FILE *output = fopen(output_filename, "w");
    FILE *metrics = fopen(metrics_filename, "w");
    if (!output || !metrics)
    {
        perror("Erro ao abrir ficheiros de saida");
        return 1;
    }

    clock_t start_time = clock();
    double wall_start = getClock();

#ifdef _OPENMP
    printf("=== SHIFTING BOTTLENECK PARALELO PARA JOB SHOP SCHEDULING ===\n");
#else
    printf("=== SHIFTING BOTTLENECK SEQUENCIAL PARA JOB SHOP SCHEDULING ===\n");
#endif
    printf("Ficheiro de entrada: %s\n", input_filename);
    printf("Ficheiro de saida: %s\n", output_filename);
    printf("Ficheiro de metricas: %s\n\n", metrics_filename);

    read_input(input_filename);      // Lê dados do problema
    shifting_bottleneck_algorithm(); // Executa o algoritmo principal

    clock_t end_time = clock();
    double wall_end = getClock();
    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    double wall_elapsed = wall_end - wall_start;

#ifdef _OPENMP
    omp_destroy_lock(&schedule_lock);
#endif

    // Escreve resultados no ficheiro de output
    fprintf(output, "%d\n", best_makespan);
    for (int j = 0; j < num_jobs; j++)
    {
        for (int m = 0; m < num_machines; m++)
        {
            fprintf(output, "%d ", best_schedule[j][m]);
        }
        fprintf(output, "\n");
    }

    // Escreve métricas no ficheiro de métricas
    fprintf(metrics, "Tempo de execucao (CPU): %.4f segundos\n", elapsed);
    fprintf(metrics, "Tempo de execucao (Wall): %.4f segundos\n", wall_elapsed);
    fprintf(metrics, "Makespan: %d\n", best_makespan);
    fprintf(metrics, "Ficheiro de entrada: %s\n", input_filename);
#ifdef _OPENMP
    fprintf(metrics, "Algoritmo: Shifting Bottleneck Paralelo\n");
    fprintf(metrics, "Threads utilizadas: %d\n", omp_get_max_threads());
    fprintf(metrics, "Speedup: %.2fx\n", elapsed > 0 ? elapsed / wall_elapsed : 1.0);
#else
    fprintf(metrics, "Algoritmo: Shifting Bottleneck Sequencial\n");
#endif

    fclose(output);
    fclose(metrics);

    // Impressão dos resultados finais
    printf("\n=== RESULTADOS ===\n");
    printf("Melhor makespan: %d\n", best_makespan);
    printf("Tempo de execucao: %.4f segundos\n", wall_elapsed);
#ifdef _OPENMP
    printf("Threads: %d, Speedup: %.2fx\n", omp_get_max_threads(),
           elapsed > 0 ? elapsed / wall_elapsed : 1.0);
#endif

    printf("\nEscalonamento final:\n");
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

    return 0;
}