#include "banker.h"

// Spawns child process
void create_processes(SystemState *state) {
    for (int i = 0; i < state->n; i++) {
        int parent_to_child[2];
        int child_to_parent[2];

        pipe(parent_to_child);
        pipe(child_to_parent);

        pid_t pid = fork();

        if (pid == 0) {
            // Child waits for instructions and responds
            close(parent_to_child[1]);
            close(child_to_parent[0]);

            state->processes[i].pipe_read = parent_to_child[0];
            state->processes[i].pipe_write = child_to_parent[1];

            int signal;
            int *resource_vector = malloc(state->m * sizeof(int));

            while (1) {
                if (read(state->processes[i].pipe_read, &signal, sizeof(int)) <= 0) break;

                if (signal == SIGNAL_EXECUTE) {
                    Instruction *instr = &state->processes[i].instructions[state->processes[i].current_instr];

                    switch (instr->type) {
                        case REQUEST:
                            write(state->processes[i].pipe_write, instr->resource_vector, state->m * sizeof(int));
                            read(state->processes[i].pipe_read, &signal, sizeof(int));
                            if (signal == SIGNAL_GRANTED) state->processes[i].current_instr++;
                            break;

                        case RELEASE:
                            write(state->processes[i].pipe_write, instr->resource_vector, state->m * sizeof(int));
                            state->processes[i].current_instr++;
                            break;

                        case COMPUTE:
                        case USE_RESOURCES:
                        case REDUCE_RESOURCES:
                        case PRINT_RESOURCES:
                            state->processes[i].current_instr++;
                            break;

                        case END:
                            write(state->processes[i].pipe_write, &signal, sizeof(int));
                            free(resource_vector);
                            close(state->processes[i].pipe_read);
                            close(state->processes[i].pipe_write);
                            exit(0);
                    }
                } else if (signal == SIGNAL_TERMINATE) {
                    break;
                }
            }

            free(resource_vector);
            close(state->processes[i].pipe_read);
            close(state->processes[i].pipe_write);
            exit(0);
        } else {
            close(parent_to_child[0]);
            close(child_to_parent[1]);

            state->processes[i].pipe_write = parent_to_child[1];
            state->processes[i].pipe_read = child_to_parent[0];
            state->processes[i].pid = pid;
        }
    }
}
// Updates master string, indicating all resources it is holding
void update_master_string(Process *process, Resource *resources, int m) {
    if (process->master_string) free(process->master_string);

    typedef struct { char *name; int count; } ResourceUsage;

    ResourceUsage *usage = NULL;
    int usage_count = 0;

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < resources[i].count; j++) {
            if (resources[i].allocated[j] == process->id) {
                int found = 0;
                for (int k = 0; k < usage_count; k++) {
                    if (strcmp(usage[k].name, resources[i].instances[j]) == 0) {
                        usage[k].count++;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    usage_count++;
                    usage = realloc(usage, usage_count * sizeof(ResourceUsage));
                    usage[usage_count - 1].name = strdup(resources[i].instances[j]);
                    usage[usage_count - 1].count = 1;
                }
            }
        }
    }

    for (int i = 0; i < usage_count - 1; i++) {
        for (int j = 0; j < usage_count - i - 1; j++) {
            if (strcmp(usage[j].name, usage[j + 1].name) > 0) {
                ResourceUsage temp = usage[j];
                usage[j] = usage[j + 1];
                usage[j + 1] = temp;
            }
        }
    }

    if (usage_count == 0) {
        process->master_string = strdup("");
    } else {
        int buffer_size = 0;
        for (int i = 0; i < usage_count; i++) {
            buffer_size += strlen(usage[i].name) + 20 + 2;
        }

        char *buffer = malloc(buffer_size);
        buffer[0] = '\0';

        for (int i = 0; i < usage_count; i++) {
            if (i > 0) strcat(buffer, ", ");

            char *count_word = number_to_english(usage[i].count);

            if (usage[i].count == 1) sprintf(buffer + strlen(buffer), "%s %s", count_word, usage[i].name);
            else sprintf(buffer + strlen(buffer), "%s %ss", count_word, usage[i].name);

            free(count_word);
        }

        process->master_string = buffer;
    }

    for (int i = 0; i < usage_count; i++) free(usage[i].name);
    free(usage);
}

// Simulates repeatedly using resources
void use_resources(Process *process, Resource *resources, int m, int times) {
    for (int t = 0; t < times; t++) {
        update_master_string(process, resources, m);
    }
}

// Simulate reducing usage temporarily
void reduce_resources(Process *process, Resource *resources, int m, int reduction) {
    int **backup = malloc(m * sizeof(int*));
    for (int i = 0; i < m; i++) {
        backup[i] = malloc(resources[i].count * sizeof(int));
        for (int j = 0; j < resources[i].count; j++) backup[i][j] = resources[i].allocated[j];
    }

    for (int i = 0; i < m; i++) {
        int reduced = 0;
        for (int j = 0; j < resources[i].count && reduced < reduction; j++) {
            if (resources[i].allocated[j] == process->id) {
                resources[i].allocated[j] = -1;
                reduced++;
            }
        }
    }

    update_master_string(process, resources, m);

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < resources[i].count; j++) resources[i].allocated[j] = backup[i][j];
        free(backup[i]);
    }
    free(backup);
}

// Handles running the current instruction for a particular process
void execute_instruction(SystemState *state, int process_id) {
    Process *p = &state->processes[process_id];
    Instruction *instr = &p->instructions[p->current_instr];

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

    switch (instr->type) {
        case COMPUTE:
            p->remaining_computation_time -= instr->computation_time;
            p->current_instr++;
            break;

        case REQUEST: {
            int signal = SIGNAL_EXECUTE;
            write(p->pipe_write, &signal, sizeof(int));

            int *request = malloc(state->m * sizeof(int));
            read(p->pipe_read, request, state->m * sizeof(int));

            int result = request_resources(state, process_id, request);

            signal = result ? SIGNAL_GRANTED : SIGNAL_DENIED;
            write(p->pipe_write, &signal, sizeof(int));

            if (result) p->remaining_computation_time--;

            free(request);
            break;
        }

        case USE_RESOURCES:
            use_resources(p, state->resources, state->m, instr->repeat_count);
            p->remaining_computation_time -= instr->computation_time;
            p->current_instr++;
            break;

        case REDUCE_RESOURCES:
            reduce_resources(p, state->resources, state->m, instr->repeat_count);
            p->remaining_computation_time -= instr->computation_time;
            p->current_instr++;
            break;

        case RELEASE: {
            int signal = SIGNAL_EXECUTE;
            write(p->pipe_write, &signal, sizeof(int));

            int *release = malloc(state->m * sizeof(int));
            read(p->pipe_read, release, state->m * sizeof(int));

            release_resources(state, process_id, release);
            p->remaining_computation_time--;

            free(release);
            break;
        }

        case PRINT_RESOURCES:
            printf("Process %d resources used: %s\n", process_id, p->master_string);
            p->remaining_computation_time--;
            p->current_instr++;
            break;

        case END: {
            int signal = SIGNAL_EXECUTE;
            write(p->pipe_write, &signal, sizeof(int));

            int *release_all = malloc(state->m * sizeof(int));
            for (int i = 0; i < state->m; i++) release_all[i] = p->allocation[i];

            release_resources(state, process_id, release_all);

            p->current_instr++;
            free(release_all);
            break;
        }
    }
}
