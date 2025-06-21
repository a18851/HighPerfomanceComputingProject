#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#define MAX_JOBS 105
#define MAX_MACHINES 105

// Global problem data
int num_jobs, num_machines;
int job_machine[MAX_JOBS][MAX_MACHINES];
int job_duration[MAX_JOBS][MAX_MACHINES];

// Solution data
int best_makespan;
int best_schedule[MAX_JOBS][MAX_MACHINES];

// Machine scheduling data
typedef struct
{
    int job;
    int operation;
    int start_time;
    int end_time;
    int machine;
    int duration;
} Operation;

Operation machine_schedule[MAX_MACHINES][MAX_JOBS * MAX_MACHINES];
int machine_op_count[MAX_MACHINES];

// Temporary arrays for calculations
int job_completion_time[MAX_JOBS];
int machine_completion_time[MAX_MACHINES];
int operation_start_time[MAX_JOBS][MAX_MACHINES];

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

void initialize_solution()
{
    // Initialize all arrays
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
}

// Calculate earliest start times for all operations
void calculate_earliest_start_times()
{
    // Initialize
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

    // Calculate earliest start times considering job precedence
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

// Calculate machine workload (total processing time)
int calculate_machine_workload(int machine)
{
    int total_workload = 0;
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

// Schedule operations on a specific machine using Shortest Processing Time first
void schedule_machine_operations(int machine)
{
    Operation operations[MAX_JOBS * MAX_MACHINES];
    int op_count = 0;

    // Collect all operations for this machine
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

    // Sort operations by earliest start time, then by shortest processing time
    for (int i = 0; i < op_count - 1; i++)
    {
        for (int k = i + 1; k < op_count; k++)
        {
            int swap = 0;

            // Primary sort: earliest start time
            if (operations[k].start_time < operations[i].start_time)
            {
                swap = 1;
            }
            // Secondary sort: shortest processing time for same start time
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

    // Schedule operations on the machine
    int current_machine_time = 0;
    machine_op_count[machine] = op_count;

    for (int i = 0; i < op_count; i++)
    {
        int earliest_start = operation_start_time[operations[i].job][operations[i].operation];
        int actual_start = (current_machine_time > earliest_start) ? current_machine_time : earliest_start;

        operations[i].start_time = actual_start;
        operations[i].end_time = actual_start + operations[i].duration;

        machine_schedule[machine][i] = operations[i];
        current_machine_time = operations[i].end_time;

        // Update the schedule
        best_schedule[operations[i].job][operations[i].operation] = actual_start;
    }

    machine_completion_time[machine] = current_machine_time;
}

// Update job completion times based on current schedule
void update_job_completion_times()
{
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

// Calculate makespan
int calculate_makespan()
{
    int makespan = 0;
    for (int j = 0; j < num_jobs; j++)
    {
        if (job_completion_time[j] > makespan)
            makespan = job_completion_time[j];
    }
    return makespan;
}

// Try to improve the schedule by rescheduling a machine
int try_improve_machine_schedule(int machine)
{
    // Save current state
    int saved_schedule[MAX_JOBS][MAX_MACHINES];
    for (int j = 0; j < num_jobs; j++)
    {
        for (int op = 0; op < num_machines; op++)
        {
            saved_schedule[j][op] = best_schedule[j][op];
        }
    }

    int saved_makespan = best_makespan;

    printf("Tentando melhorar escalonamento da maquina %d...\n", machine);

    // Recalculate earliest start times
    calculate_earliest_start_times();

    // Reschedule this machine
    schedule_machine_operations(machine);

    // Update other machines to respect the new schedule
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
        printf("Melhoria encontrada! Novo makespan: %d\n", best_makespan);
        return 1; // Improvement found
    }
    else
    {
        // Restore previous state
        for (int j = 0; j < num_jobs; j++)
        {
            for (int op = 0; op < num_machines; op++)
            {
                best_schedule[j][op] = saved_schedule[j][op];
            }
        }
        best_makespan = saved_makespan;
        printf("Nenhuma melhoria encontrada para maquina %d\n", machine);
        return 0; // No improvement
    }
}

// Main Shifting Bottleneck Algorithm - Sequential Version
void shifting_bottleneck_algorithm()
{
    printf("=== ALGORITMO SHIFTING BOTTLENECK SEQUENCIAL ===\n\n");

    initialize_solution();

    // Phase 1: Build initial schedule
    printf("Fase 1: Construindo escalonamento inicial...\n");
    calculate_earliest_start_times();

    // Schedule all machines in order of workload (heaviest first)
    int machine_order[MAX_MACHINES];
    int machine_workload[MAX_MACHINES];

    for (int m = 0; m < num_machines; m++)
    {
        machine_order[m] = m;
        machine_workload[m] = calculate_machine_workload(m);
    }

    // Sort machines by workload (descending)
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

    // Schedule machines in order - SEQUENTIAL
    for (int i = 0; i < num_machines; i++)
    {
        int machine = machine_order[i];
        printf("\nEscalonando maquina %d...\n", machine);
        schedule_machine_operations(machine);
        update_job_completion_times();

        // Recalculate earliest start times for remaining operations
        calculate_earliest_start_times();
    }

    update_job_completion_times();
    best_makespan = calculate_makespan();
    printf("\nMakespan inicial: %d\n", best_makespan);

    // Phase 2: Improvement phase - SEQUENTIAL
    printf("\nFase 2: Melhorando escalonamento (sequencial)...\n");
    int iteration = 0;
    int improved = 1;

    while (improved && iteration < 10)
    {
        improved = 0;
        iteration++;
        printf("\nIteracao %d de melhoria:\n", iteration);

        // Try to improve each machine sequentially
        for (int m = 0; m < num_machines; m++)
        {
            if (try_improve_machine_schedule(m))
            {
                improved = 1;
            }
        }

        if (!improved)
        {
            printf("Nenhuma melhoria encontrada nesta iteracao.\n");
        }
    }

    printf("\nAlgoritmo Shifting Bottleneck Sequencial concluido.\n");
    printf("Makespan final: %d\n", best_makespan);
    printf("Iteracoes de melhoria: %d\n", iteration);
}

int main(int argc, char **argv)
{
    // Check command line arguments
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

    printf("=== SHIFTING BOTTLENECK SEQUENCIAL PARA JOB SHOP SCHEDULING ===\n");
    printf("Arquivo de entrada: %s\n", input_filename);
    printf("Arquivo de saida: %s\n", output_filename);
    printf("Arquivo de metricas: %s\n\n", metrics_filename);

    read_input(input_filename);
    shifting_bottleneck_algorithm();

    clock_t end_time = clock();
    double elapsed = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    // Output
    fprintf(output, "%d\n", best_makespan);
    for (int j = 0; j < num_jobs; j++)
    {
        for (int m = 0; m < num_machines; m++)
        {
            fprintf(output, "%d ", best_schedule[j][m]);
        }
        fprintf(output, "\n");
    }
    fprintf(metrics, "Tempo de execucao: %.4f segundos\n", elapsed);
    fprintf(metrics, "Makespan: %d\n", best_makespan);
    fprintf(metrics, "Algoritmo: Shifting Bottleneck Sequencial\n");
    fprintf(metrics, "Arquivo de entrada: %s\n", input_filename);

    fclose(output);
    fclose(metrics);

    printf("\n=== RESULTADOS ===\n");
    printf("Melhor makespan: %d\n", best_makespan);
    printf("Tempo de execucao: %.4f segundos\n", elapsed);

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