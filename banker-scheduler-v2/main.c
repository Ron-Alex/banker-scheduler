/*
 * COSC 3360 Operating Systems
 * Assignment 2 - Banker's Algorithm
 * Ron Alex
 * 
 * Compile: gcc -o banker main.c banker.c process.c scheduler.c utils.c -lpthread
 * Run: ./banker sample.txt sample_words.txt
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <sys/types.h>
 #include <sys/wait.h>
 #include <sys/ipc.h>
 #include <sys/sem.h>
 #include <fcntl.h>
 #include <signal.h>
 #include "banker.h"
 
 // Global variables
 int *semaphores = NULL;
 int sem_count = 0;
 int shmid = 0;
 
 // Function prototypes
 void init_system(SystemState *state, char *system_file, char *resources_file);
 void cleanup_system(SystemState *state);
 void run_simulation(SystemState *state, int scheduler_type);
 void reset_state(SystemState *state);
 int count_missed_deadlines(SystemState *state);
 
 int main(int argc, char *argv[]) {
     if (argc != 3) {
         printf("Usage: %s <system_file> <resources_file>\n", argv[0]);
         return 1;
     }
     
     // Initialize system state
     SystemState state;
     memset(&state, 0, sizeof(SystemState));
     init_system(&state, argv[1], argv[2]);
     
     printf("========== EDF Scheduler with SJF Tie-Breaking ==========\n");
     run_simulation(&state, SCHEDULER_EDF);
     
     int edf_missed = count_missed_deadlines(&state);
     
     reset_state(&state);
     
     printf("\n========== LLF Scheduler with LJF Tie-Breaking ==========\n");
     run_simulation(&state, SCHEDULER_LLF);
     
     int llf_missed = count_missed_deadlines(&state);
     
     printf("\n========== Results Comparison ==========\n");
     printf("EDF missed deadlines: %d\n", edf_missed);
     printf("LLF missed deadlines: %d\n", llf_missed);
     
     if (edf_missed < llf_missed) {
         printf("EDF scheduling yields fewer deadline misses.\n");
     } else if (llf_missed < edf_missed) {
         printf("LLF scheduling yields fewer deadline misses.\n");
     } else {
         printf("Both scheduling techniques yield the same number of deadline misses.\n");
     }
     
     // Clean up
     cleanup_system(&state);
     
     return 0;
 }
 
 // Initializes the entire system
 void init_system(SystemState *state, char *system_file, char *resources_file) {
     read_system_state(state, system_file);
     read_resource_instances(state, resources_file);
     
     init_semaphores(state);
     
     create_processes(state);
 }
 
// Cleans up resources
 void cleanup_system(SystemState *state) {
     for (int i = 0; i < state->m; i++) {
         free(state->resources[i].name);
         for (int j = 0; j < state->resources[i].count; j++) {
             free(state->resources[i].instances[j]);
         }
         free(state->resources[i].instances);
     }
     free(state->resources);
     
     for (int i = 0; i < state->n; i++) {
         free(state->processes[i].max);
         free(state->processes[i].allocation);
         free(state->processes[i].need);
         free(state->processes[i].master_string);
         
         close(state->processes[i].pipe_read);
         close(state->processes[i].pipe_write);
         
         for (int j = 0; j < state->processes[i].num_instructions; j++) {
             Instruction *instr = &state->processes[i].instructions[j];
             if (instr->type == REQUEST || instr->type == RELEASE) {
                 free(instr->resource_vector);
             }
         }
         free(state->processes[i].instructions);
     }
     free(state->processes);
     
     free(state->available);
     
     for (int i = 0; i < sem_count; i++) {
         semctl(semaphores[i], 0, IPC_RMID);
     }
     free(semaphores);
 }
 
 // Runs the simulation
 void run_simulation(SystemState *state, int scheduler_type) {
     int time = 0;
     int active_processes = state->n;
     int *last_serviced = malloc(state->n * sizeof(int));
     
     for (int i = 0; i < state->n; i++) {
         last_serviced[i] = -1;
     }
     
     while (active_processes > 0) {
         printf("\nTime: %d\n", time);
         
         for (int i = 0; i < state->n; i++) {
             Process *p = &state->processes[i];
             if (p->current_instr < p->num_instructions) {
                 if (p->initial_deadline > 0) {
                     p->remaining_deadline--;
                     
                     if (p->remaining_deadline < 0 && !p->missed_deadline) {
                         p->missed_deadline = 1;
                         printf("Process %d missed its deadline at time %d\n", i, time);
                     }
                 }
             }
         }
         
         int next_process;
         if (scheduler_type == SCHEDULER_EDF) {
             next_process = edf_scheduler(state, last_serviced);
         } else {
             next_process = llf_scheduler(state, last_serviced);
         }
         
         if (next_process != -1) {
             Process *p = &state->processes[next_process];
             
             printf("Executing instruction for Process %d: ", next_process);
             execute_instruction(state, next_process);
             
             last_serviced[next_process] = time;
             
             if (p->current_instr >= p->num_instructions) {
                 printf("Process %d completed at time %d\n", next_process, time);
                 active_processes--;
             }
             
             print_system_state(state);
         } else {
             printf("No eligible process to execute at time %d\n", time);
         }
         
         time++;
     }
     
     printf("Simulation completed at time %d\n", time);
     free(last_serviced);
 }
 // Resets entire state to rerun simulation
 void reset_state(SystemState *state) {
     for (int i = 0; i < state->n; i++) {
         Process *p = &state->processes[i];
         p->current_instr = 0;
         p->remaining_computation_time = p->initial_computation_time;
         p->remaining_deadline = p->initial_deadline;
         p->missed_deadline = 0;
         
         for (int j = 0; j < state->m; j++) {
             p->allocation[j] = 0;
             p->need[j] = p->max[j]; 
         }
         
         free(p->master_string);
         p->master_string = strdup("");
     }
     
     for (int i = 0; i < state->m; i++) {
         state->available[i] = state->initial_available[i];
     }
 }
 // Returns count of processes that have missed thier deadline
 int count_missed_deadlines(SystemState *state) {
     int missed = 0;
     for (int i = 0; i < state->n; i++) {
         if (state->processes[i].missed_deadline)
             missed++;
     }
     return missed;
 }