/*
 * COSC 3360 Operating Systems - Spring 2025
 * Assignment 2 - Banker's Algorithm with EDF/LLF Scheduling
 * 
 * To compile: gcc -o banker main.c banker.c process.c scheduler.c utils.c -lpthread
 * To run: ./banker <system_file> <resources_file>
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
 
 // Global variables for IPC and synchronization
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
     
     // Run EDF simulation
     printf("========== EDF Scheduler with SJF Tie-Breaking ==========\n");
     run_simulation(&state, SCHEDULER_EDF);
     
     // Count EDF missed deadlines
     int edf_missed = count_missed_deadlines(&state);
     
     // Reset state for LLF simulation
     reset_state(&state);
     
     // Run LLF simulation
     printf("\n========== LLF Scheduler with LJF Tie-Breaking ==========\n");
     run_simulation(&state, SCHEDULER_LLF);
     
     // Count LLF missed deadlines
     int llf_missed = count_missed_deadlines(&state);
     
     // Compare results
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
 
 void init_system(SystemState *state, char *system_file, char *resources_file) {
     // Read system state from files
     read_system_state(state, system_file);
     read_resource_instances(state, resources_file);
     
     // Initialize semaphores for resource instances
     init_semaphores(state);
     
     // Create processes and set up pipes
     create_processes(state);
 }
 
 void cleanup_system(SystemState *state) {
     // Clean up resources
     for (int i = 0; i < state->m; i++) {
         free(state->resources[i].name);
         for (int j = 0; j < state->resources[i].count; j++) {
             free(state->resources[i].instances[j]);
         }
         free(state->resources[i].instances);
     }
     free(state->resources);
     
     // Clean up processes
     for (int i = 0; i < state->n; i++) {
         free(state->processes[i].max);
         free(state->processes[i].allocation);
         free(state->processes[i].need);
         free(state->processes[i].master_string);
         
         // Close pipes
         close(state->processes[i].pipe_read);
         close(state->processes[i].pipe_write);
         
         // Free instructions
         for (int j = 0; j < state->processes[i].num_instructions; j++) {
             Instruction *instr = &state->processes[i].instructions[j];
             if (instr->type == REQUEST || instr->type == RELEASE) {
                 free(instr->resource_vector);
             }
         }
         free(state->processes[i].instructions);
     }
     free(state->processes);
     
     // Free available resources
     free(state->available);
     
     // Clean up semaphores
     for (int i = 0; i < sem_count; i++) {
         semctl(semaphores[i], 0, IPC_RMID);
     }
     free(semaphores);
 }
 
 void run_simulation(SystemState *state, int scheduler_type) {
     int time = 0;
     int active_processes = state->n;
     int *last_serviced = malloc(state->n * sizeof(int));
     
     // Initialize last_serviced array to track when each process was last serviced
     for (int i = 0; i < state->n; i++) {
         last_serviced[i] = -1;  // Never serviced
     }
     
     // Continue until all processes complete
     while (active_processes > 0) {
         // Print current time
         printf("\nTime: %d\n", time);
         
         // Update deadlines and laxities
         for (int i = 0; i < state->n; i++) {
             Process *p = &state->processes[i];
             if (p->current_instr < p->num_instructions) {
                 if (p->initial_deadline > 0) {
                     p->remaining_deadline--;
                     
                     // Check for missed deadlines
                     if (p->remaining_deadline < 0 && !p->missed_deadline) {
                         p->missed_deadline = 1;
                         printf("Process %d missed its deadline at time %d\n", i, time);
                     }
                 }
             }
         }
         
         // Select next process using scheduler
         int next_process;
         if (scheduler_type == SCHEDULER_EDF) {
             next_process = edf_scheduler(state, last_serviced);
         } else {
             next_process = llf_scheduler(state, last_serviced);
         }
         
         if (next_process != -1) {
             Process *p = &state->processes[next_process];
             
             // Execute the current instruction
             printf("Executing instruction for Process %d: ", next_process);
             execute_instruction(state, next_process);
             
             // Mark this process as serviced
             last_serviced[next_process] = time;
             
             // Check if process is complete
             if (p->current_instr >= p->num_instructions) {
                 printf("Process %d completed at time %d\n", next_process, time);
                 active_processes--;
             }
             
             // Print system state
             print_system_state(state);
         } else {
             printf("No eligible process to execute at time %d\n", time);
         }
         
         time++;
     }
     
     printf("Simulation completed at time %d\n", time);
     free(last_serviced);
 }
 
 void reset_state(SystemState *state) {
     // Reset process states
     for (int i = 0; i < state->n; i++) {
         Process *p = &state->processes[i];
         p->current_instr = 0;
         p->remaining_computation_time = p->initial_computation_time;
         p->remaining_deadline = p->initial_deadline;
         p->missed_deadline = 0;
         
         // Reset allocation and need
         for (int j = 0; j < state->m; j++) {
             p->allocation[j] = 0;
             p->need[j] = p->max[j];
         }
         
         // Reset master string
         free(p->master_string);
         p->master_string = strdup("");
     }
     
     // Reset available resources
     for (int i = 0; i < state->m; i++) {
         state->available[i] = state->initial_available[i];
     }
 }
 
 int count_missed_deadlines(SystemState *state) {
     int missed = 0;
     for (int i = 0; i < state->n; i++) {
         if (state->processes[i].missed_deadline)
             missed++;
     }
     return missed;
 }