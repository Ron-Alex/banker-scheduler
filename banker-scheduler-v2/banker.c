#include "banker.h"

void read_system_state(SystemState *state, char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening system file");
        exit(1);
    }
    
    // Read m and n
    fscanf(fp, "%d", &state->m);
    fscanf(fp, "%d", &state->n);
    
    // Allocate memory for resources, available, etc.
    state->available = malloc(state->m * sizeof(int));
    state->initial_available = malloc(state->m * sizeof(int));
    state->resources = malloc(state->m * sizeof(Resource));
    state->processes = malloc(state->n * sizeof(Process));
    
    // Read available vector
    for (int i = 0; i < state->m; i++) {
        fscanf(fp, "%d", &state->available[i]);
        state->initial_available[i] = state->available[i];
    }
    
    // Read maximum demand matrix
    for (int i = 0; i < state->n; i++) {
        state->processes[i].id = i;
        state->processes[i].max = malloc(state->m * sizeof(int));
        state->processes[i].allocation = calloc(state->m, sizeof(int));
        state->processes[i].need = malloc(state->m * sizeof(int));
        state->processes[i].master_string = strdup("");
        state->processes[i].current_instr = 0;
        state->processes[i].missed_deadline = 0;
        
        for (int j = 0; j < state->m; j++) {
            fscanf(fp, "%d", &state->processes[i].max[j]);
            state->processes[i].need[j] = state->processes[i].max[j];
        }
    }
    
    // Skip any whitespace
    char c;
    while ((c = fgetc(fp)) != EOF && (c == ' ' || c == '\n' || c == '\t'));
    ungetc(c, fp);
    
    // Read process instructions
    for (int i = 0; i < state->n; i++) {
        char buffer[100];
        
        // Skip "process_X:" line
        fscanf(fp, "%s", buffer);
        
        // Read deadline and computation time
        fscanf(fp, "%d", &state->processes[i].initial_deadline);
        fscanf(fp, "%d", &state->processes[i].initial_computation_time);
        
        state->processes[i].remaining_deadline = state->processes[i].initial_deadline;
        state->processes[i].remaining_computation_time = state->processes[i].initial_computation_time;
        
        // Parse instructions
        parse_instructions(fp, &state->processes[i]);
    }
    
    fclose(fp);
}

void read_resource_instances(SystemState *state, char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening resources file");
        exit(1);
    }
    
    for (int i = 0; i < state->m; i++) {
        char line[2048];
        if (fgets(line, sizeof(line), fp) == NULL) {
            fprintf(stderr, "Error reading line %d from resources file\n", i+1);
            exit(1);
        }
        
        // Parse resource name and instances
        parse_resource_line(line, &state->resources[i]);
        
        // Initialize allocation tracking
        state->resources[i].allocated = malloc(state->resources[i].count * sizeof(int));
        for (int j = 0; j < state->resources[i].count; j++) {
            state->resources[i].allocated[j] = -1;  // -1 means not allocated
        }
    }
    
    fclose(fp);
}

void init_semaphores(SystemState *state) {
    // Count total number of resource instances
    sem_count = 0;
    for (int i = 0; i < state->m; i++) {
        sem_count += state->resources[i].count;
    }
    
    // Create semaphores
    semaphores = malloc(sem_count * sizeof(int));
    int sem_idx = 0;
    
    for (int i = 0; i < state->m; i++) {
        for (int j = 0; j < state->resources[i].count; j++) {
            key_t key = ftok(".", (i+1) * 100 + j);
            semaphores[sem_idx] = semget(key, 1, IPC_CREAT | 0666);
            
            if (semaphores[sem_idx] == -1) {
                perror("Error creating semaphore");
                exit(1);
            }
            
            // Initialize semaphore to 1 (available)
            union semun arg;
            arg.val = 1;
            semctl(semaphores[sem_idx], 0, SETVAL, arg);
            
            sem_idx++;
        }
    }
}

int is_safe_state(SystemState *state) {
    int *work = malloc(state->m * sizeof(int));
    int *finish = calloc(state->n, sizeof(int));
    
    // Copy available resources to work
    for (int i = 0; i < state->m; i++) {
        work[i] = state->available[i];
    }
    
    // Find a process that can finish
    int found;
    do {
        found = 0;
        for (int i = 0; i < state->n; i++) {
            if (!finish[i]) {
                int can_allocate = 1;
                
                // Check if all needed resources are available
                for (int j = 0; j < state->m; j++) {
                    if (state->processes[i].need[j] > work[j]) {
                        can_allocate = 0;
                        break;
                    }
                }
                
                if (can_allocate) {
                    // Process can finish, release its resources
                    for (int j = 0; j < state->m; j++) {
                        work[j] += state->processes[i].allocation[j];
                    }
                    
                    finish[i] = 1;
                    found = 1;
                }
            }
        }
    } while (found);
    
    // Check if all processes can finish
    int safe = 1;
    for (int i = 0; i < state->n; i++) {
        if (!finish[i]) {
            safe = 0;
            break;
        }
    }
    
    free(work);
    free(finish);
    
    return safe;
}

int request_resources(SystemState *state, int process_id, int *request) {
    Process *p = &state->processes[process_id];
    
    // Check if request exceeds need
    for (int i = 0; i < state->m; i++) {
        if (request[i] > p->need[i]) {
            printf("Request exceeds need for process %d\n", process_id);
            return 0;  // Error: request exceeds need
        }
    }
    
    // Check if request exceeds available
    for (int i = 0; i < state->m; i++) {
        if (request[i] > state->available[i]) {
            printf("Request exceeds available resources for process %d\n", process_id);
            return 0;  // Error: request exceeds available
        }
    }
    
    // Try to allocate
    for (int i = 0; i < state->m; i++) {
        state->available[i] -= request[i];
        p->allocation[i] += request[i];
        p->need[i] -= request[i];
    }
    
    // Allocate specific resource instances
    for (int i = 0; i < state->m; i++) {
        if (request[i] > 0) {
            int allocated = 0;
            
            // Find available instances
            for (int j = 0; j < state->resources[i].count && allocated < request[i]; j++) {
                if (state->resources[i].allocated[j] == -1) {
                    // Allocate this instance to the process
                    state->resources[i].allocated[j] = process_id;
                    allocated++;
                }
            }
        }
    }
    
    // Check if system is still in safe state
    if (is_safe_state(state)) {
        return 1;  // Request granted
    }
    
    // If not safe, rollback
    for (int i = 0; i < state->m; i++) {
        int released = 0;
        
        // Release specific instances
        for (int j = 0; j < state->resources[i].count && released < request[i]; j++) {
            if (state->resources[i].allocated[j] == process_id) {
                state->resources[i].allocated[j] = -1;
                released++;
            }
        }
        
        state->available[i] += request[i];
        p->allocation[i] -= request[i];
        p->need[i] += request[i];
    }
    
    printf("Request denied for process %d (would lead to unsafe state)\n", process_id);
    return 0;  // Request denied
}

void release_resources(SystemState *state, int process_id, int *release) {
    Process *p = &state->processes[process_id];
    
    // Check if release is valid
    for (int i = 0; i < state->m; i++) {
        if (release[i] > p->allocation[i]) {
            printf("Warning: Process %d trying to release more resources than allocated\n", process_id);
            release[i] = p->allocation[i];  // Adjust to actual allocation
        }
    }
    
    // Release resources
    for (int i = 0; i < state->m; i++) {
        if (release[i] > 0) {
            int released = 0;
            
            // Release specific instances
            for (int j = 0; j < state->resources[i].count && released < release[i]; j++) {
                if (state->resources[i].allocated[j] == process_id) {
                    state->resources[i].allocated[j] = -1;
                    released++;
                }
            }
            
            state->available[i] += release[i];
            p->allocation[i] -= release[i];
            p->need[i] += release[i];
        }
    }
}

void print_system_state(SystemState *state) {
    printf("\nSystem State:\n");
    
    // Print available resources
    printf("Available resources: [");
    for (int i = 0; i < state->m; i++) {
        printf("%d", state->available[i]);
        if (i < state->m - 1) printf(", ");
    }
    printf("]\n");
    
    // Print allocation matrix
    printf("Allocation matrix:\n");
    for (int i = 0; i < state->n; i++) {
        printf("  Process %d: [", i);
        for (int j = 0; j < state->m; j++) {
            printf("%d", state->processes[i].allocation[j]);
            if (j < state->m - 1) printf(", ");
        }
        printf("]\n");
    }
    
    // Print need matrix
    printf("Need matrix:\n");
    for (int i = 0; i < state->n; i++) {
        printf("  Process %d: [", i);
        for (int j = 0; j < state->m; j++) {
            printf("%d", state->processes[i].need[j]);
            if (j < state->m - 1) printf(", ");
        }
        printf("]\n");
    }
    
    // Print deadline information
    printf("Deadlines:\n");
    for (int i = 0; i < state->n; i++) {
        printf("  Process %d: Remaining deadline = %d, ", i, state->processes[i].remaining_deadline);
        printf("Remaining computation time = %d", state->processes[i].remaining_computation_time);
        if (state->processes[i].missed_deadline) {
            printf(" (MISSED DEADLINE)");
        }
        printf("\n");
    }
}