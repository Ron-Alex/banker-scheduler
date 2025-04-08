#include "banker.h"

void create_processes(SystemState *state) {
    for (int i = 0; i < state->n; i++) {
        // Create pipes for communication
        int parent_to_child[2];
        int child_to_parent[2];
        
        pipe(parent_to_child);
        pipe(child_to_parent);
        
        pid_t pid = fork();
        
        if (pid == 0) {
            // Child process
            close(parent_to_child[1]);  // Close write end of parent->child
            close(child_to_parent[0]);  // Close read end of child->parent
            
            // Set up pipes for reading/writing
            state->processes[i].pipe_read = parent_to_child[0];
            state->processes[i].pipe_write = child_to_parent[1];
            
            // Child process waits for instructions from parent
            int signal;
            int *resource_vector = malloc(state->m * sizeof(int));
            
            while (1) {
                // Wait for signal from parent
                if (read(state->processes[i].pipe_read, &signal, sizeof(int)) <= 0) {
                    break;  // Parent closed the pipe, exit
                }
                
                if (signal == SIGNAL_EXECUTE) {
                    // Get current instruction
                    Instruction *instr = &state->processes[i].instructions[state->processes[i].current_instr];
                    
                    switch (instr->type) {
                        case REQUEST:
                            // Send request to parent
                            write(state->processes[i].pipe_write, instr->resource_vector, state->m * sizeof(int));
                            
                            // Wait for response
                            read(state->processes[i].pipe_read, &signal, sizeof(int));
                            
                            // If granted, increment current instruction
                            if (signal == SIGNAL_GRANTED) {
                                state->processes[i].current_instr++;
                            }
                            break;
                            
                        case RELEASE:
                            // Send release to parent
                            write(state->processes[i].pipe_write, instr->resource_vector, state->m * sizeof(int));
                            state->processes[i].current_instr++;
                            break;
                            
                        case COMPUTE:
                        case USE_RESOURCES:
                        case REDUCE_RESOURCES:
                        case PRINT_RESOURCES:
                            // These are handled by the parent
                            state->processes[i].current_instr++;
                            break;
                            
                        case END:
                            // Signal completion and terminate
                            write(state->processes[i].pipe_write, &signal, sizeof(int));
                            free(resource_vector);
                            close(state->processes[i].pipe_read);
                            close(state->processes[i].pipe_write);
                            exit(0);
                    }
                } else if (signal == SIGNAL_TERMINATE) {
                    // Parent is terminating the process
                    break;
                }
            }
            
            free(resource_vector);
            close(state->processes[i].pipe_read);
            close(state->processes[i].pipe_write);
            exit(0);
        } else {
            // Parent process
            close(parent_to_child[0]);  // Close read end of parent->child
            close(child_to_parent[1]);  // Close write end of child->parent
            
            // Set up pipes for reading/writing
            state->processes[i].pipe_write = parent_to_child[1];
            state->processes[i].pipe_read = child_to_parent[0];
            state->processes[i].pid = pid;
        }
    }
}

void update_master_string(Process *process, Resource *resources, int m) {
    // Free old master string
    if (process->master_string) {
        free(process->master_string);
    }
    
    // Create a structure to track resources
    typedef struct {
        char *name;
        int count;
    } ResourceUsage;
    
    // Collect all allocated resources and their counts
    ResourceUsage *usage = NULL;
    int usage_count = 0;
    
    for (int i = 0; i < m; i++) {
        // Count how many instances of this resource type are allocated to this process
        for (int j = 0; j < resources[i].count; j++) {
            if (resources[i].allocated[j] == process->id) {
                // Check if we already have this instance in our usage list
                int found = 0;
                for (int k = 0; k < usage_count; k++) {
                    if (strcmp(usage[k].name, resources[i].instances[j]) == 0) {
                        usage[k].count++;
                        found = 1;
                        break;
                    }
                }
                
                if (!found) {
                    // Add new entry
                    usage_count++;
                    usage = realloc(usage, usage_count * sizeof(ResourceUsage));
                    usage[usage_count - 1].name = strdup(resources[i].instances[j]);
                    usage[usage_count - 1].count = 1;
                }
            }
        }
    }
    
    // Sort usage alphabetically by name
    for (int i = 0; i < usage_count - 1; i++) {
        for (int j = 0; j < usage_count - i - 1; j++) {
            if (strcmp(usage[j].name, usage[j + 1].name) > 0) {
                // Swap
                ResourceUsage temp = usage[j];
                usage[j] = usage[j + 1];
                usage[j + 1] = temp;
            }
        }
    }
    
    // Build master string
    if (usage_count == 0) {
        process->master_string = strdup("");
    } else {
        // Calculate required buffer size
        int buffer_size = 0;
        for (int i = 0; i < usage_count; i++) {
            // Count word plus count word plus pluralization if needed plus comma/space
            buffer_size += strlen(usage[i].name) + 20 + 2;
        }
        
        char *buffer = malloc(buffer_size);
        buffer[0] = '\0';
        
        for (int i = 0; i < usage_count; i++) {
            if (i > 0) {
                strcat(buffer, ", ");
            }
            
            char *count_word = number_to_english(usage[i].count);
            
            if (usage[i].count == 1) {
                sprintf(buffer + strlen(buffer), "%s %s", count_word, usage[i].name);
            } else {
                sprintf(buffer + strlen(buffer), "%s %ss", count_word, usage[i].name);
            }
            
            free(count_word);
        }
        
        process->master_string = buffer;
    }
    
    // Clean up
    for (int i = 0; i < usage_count; i++) {
        free(usage[i].name);
    }
    free(usage);
}

void use_resources(Process *process, Resource *resources, int m, int times) {
    // This doesn't actually change the resource allocation,
    // it just updates the master string to indicate resource usage
    for (int t = 0; t < times; t++) {
        update_master_string(process, resources, m);
    }
}

void reduce_resources(Process *process, Resource *resources, int m, int reduction) {
    // Similar to use_resources, but simulates reducing usage
    // We'll implement this by temporarily modifying the allocation
    
    // Create a backup of the allocation
    int **backup = malloc(m * sizeof(int*));
    for (int i = 0; i < m; i++) {
        backup[i] = malloc(resources[i].count * sizeof(int));
        for (int j = 0; j < resources[i].count; j++) {
            backup[i][j] = resources[i].allocated[j];
        }
    }
    
    // Simulate reducing usage by temporarily unallocating resources
    for (int i = 0; i < m; i++) {
        int reduced = 0;
        for (int j = 0; j < resources[i].count && reduced < reduction; j++) {
            if (resources[i].allocated[j] == process->id) {
                resources[i].allocated[j] = -1;  // Temporarily unallocate
                reduced++;
            }
        }
    }
    
    // Update master string reflecting reduced resources
    update_master_string(process, resources, m);
    
    // Restore the original allocation
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < resources[i].count; j++) {
            resources[i].allocated[j] = backup[i][j];
        }
        free(backup[i]);
    }
    free(backup);
}

void execute_instruction(SystemState *state, int process_id) {
    Process *p = &state->processes[process_id];
    Instruction *instr = &p->instructions[p->current_instr];
    
    // Print instruction being executed
    switch (instr->type) {
        case COMPUTE:
            printf("compute %d\n", instr->computation_time);
            break;
            
        case REQUEST:
            printf("request [");
            for (int i = 0; i < state->m; i++) {
                printf("%d", instr->resource_vector[i]);
                if (i < state->m - 1) printf(", ");
            }
            printf("]\n");
            break;
            
        case USE_RESOURCES:
            printf("use_resources %d %d\n", instr->computation_time, instr->repeat_count);
            break;
            
        case REDUCE_RESOURCES:
            printf("reduce_resources %d %d\n", instr->computation_time, instr->repeat_count);
            break;
            
        case RELEASE:
            printf("release [");
            for (int i = 0; i < state->m; i++) {
                printf("%d", instr->resource_vector[i]);
                if (i < state->m - 1) printf(", ");
            }
            printf("]\n");
            break;
            
        case PRINT_RESOURCES:
            printf("print_resources_used\n");
            break;
            
        case END:
            printf("end\n");
            break;
    }
    
    // Execute the instruction
    switch (instr->type) {
        case COMPUTE:
            // Simulate computation (just decrease remaining time)
            p->remaining_computation_time -= instr->computation_time;
            p->current_instr++;
            break;
            
        case REQUEST:
            {
                // Send signal to process
                int signal = SIGNAL_EXECUTE;
                write(p->pipe_write, &signal, sizeof(int));
                
                // Receive request
                int *request = malloc(state->m * sizeof(int));
                read(p->pipe_read, request, state->m * sizeof(int));
                
                // Try to allocate
                int result = request_resources(state, process_id, request);
                
                // Send result back to process
                signal = result ? SIGNAL_GRANTED : SIGNAL_DENIED;
                write(p->pipe_write, &signal, sizeof(int));
                
                // Update computation time only if request is granted
                if (result) {
                    p->remaining_computation_time--;
                }
                
                free(request);
            }
            break;
            
        case USE_RESOURCES:
            // Use resources (update master string)
            use_resources(p, state->resources, state->m, instr->repeat_count);
            p->remaining_computation_time -= instr->computation_time;
            p->current_instr++;
            break;
            
        case REDUCE_RESOURCES:
            // Reduce resources (update master string)
            reduce_resources(p, state->resources, state->m, instr->repeat_count);
            p->remaining_computation_time -= instr->computation_time;
            p->current_instr++;
            break;
            
        case RELEASE:
            {
                // Send signal to process
                int signal = SIGNAL_EXECUTE;
                write(p->pipe_write, &signal, sizeof(int));
                
                // Receive release
                int *release = malloc(state->m * sizeof(int));
                read(p->pipe_read, release, state->m * sizeof(int));
                
                // Release resources
                release_resources(state, process_id, release);
                p->remaining_computation_time--;
                
                free(release);
            }
            break;
            
        case PRINT_RESOURCES:
            // Print master string
            printf("Process %d resources used: %s\n", process_id, p->master_string);
            p->remaining_computation_time--;
            p->current_instr++;
            break;
            
        case END:
            {
                // Send signal to process
                int signal = SIGNAL_EXECUTE;
                write(p->pipe_write, &signal, sizeof(int));
                
                // Release all resources
                int *release_all = malloc(state->m * sizeof(int));
                for (int i = 0; i < state->m; i++) {
                    release_all[i] = p->allocation[i];
                }
                
                release_resources(state, process_id, release_all);
                p->current_instr++;
                
                free(release_all);
            }
            break;
    }
}
