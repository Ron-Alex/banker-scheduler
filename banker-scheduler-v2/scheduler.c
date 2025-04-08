#include "banker.h"

// EDF Scheduler with SJF tie-breaking
int edf_scheduler(SystemState *state, int *last_serviced) {
    int min_deadline = INT_MAX;
    int min_process = -1;
    int current_time = 0;  // Approximation of current time
    
    // Find the most recent service time (for round-robin behavior)
    for (int i = 0; i < state->n; i++) {
        if (last_serviced[i] > current_time) {
            current_time = last_serviced[i];
        }
    }
    
    // First pass: find the minimum deadline
    for (int i = 0; i < state->n; i++) {
        Process *p = &state->processes[i];
        
        // Skip processes that have completed or aren't ready
        if (p->current_instr >= p->num_instructions) {
            continue;
        }
        
        // Skip processes that were serviced in the most recent round
        if (last_serviced[i] == current_time) {
            continue;
        }
        
        if (p->remaining_deadline < min_deadline) {
            min_deadline = p->remaining_deadline;
            min_process = i;
        }
    }
    
    // If no process was found, allow processes from the most recent round
    if (min_process == -1) {
        for (int i = 0; i < state->n; i++) {
            Process *p = &state->processes[i];
            
            // Skip processes that have completed
            if (p->current_instr >= p->num_instructions) {
                continue;
            }
            
            if (p->remaining_deadline < min_deadline) {
                min_deadline = p->remaining_deadline;
                min_process = i;
            }
        }
    }
    
    // Second pass: check for ties and apply SJF
    if (min_process != -1) {
        for (int i = 0; i < state->n; i++) {
            Process *p = &state->processes[i];
            
            // Skip processes that have completed or aren't in current round
            if (p->current_instr >= p->num_instructions) {
                continue;
            }
            
            if (last_serviced[i] == current_time && last_serviced[min_process] != current_time) {
                continue;
            }
            
            if (p->remaining_deadline == min_deadline && i != min_process) {
                // Tie-breaking: choose process with shorter remaining computation time (SJF)
                if (p->remaining_computation_time < state->processes[min_process].remaining_computation_time) {
                    min_process = i;
                }
            }
        }
    }
    
    return min_process;
}

// LLF Scheduler with LJF tie-breaking
int llf_scheduler(SystemState *state, int *last_serviced) {
    int min_laxity = INT_MAX;
    int min_process = -1;
    int current_time = 0;  // Approximation of current time
    
    // Find the most recent service time (for round-robin behavior)
    for (int i = 0; i < state->n; i++) {
        if (last_serviced[i] > current_time) {
            current_time = last_serviced[i];
        }
    }
    
    // First pass: find the minimum laxity
    for (int i = 0; i < state->n; i++) {
        Process *p = &state->processes[i];
        
        // Skip processes that have completed or aren't ready
        if (p->current_instr >= p->num_instructions) {
            continue;
        }
        
        // Skip processes that were serviced in the most recent round
        if (last_serviced[i] == current_time) {
            continue;
        }
        
        // Calculate laxity (deadline - remaining computation time)
        int laxity = p->remaining_deadline - p->remaining_computation_time;
        
        if (laxity < min_laxity) {
            min_laxity = laxity;
            min_process = i;
        }
    }
    
    // If no process was found, allow processes from the most recent round
    if (min_process == -1) {
        for (int i = 0; i < state->n; i++) {
            Process *p = &state->processes[i];
            
            // Skip processes that have completed
            if (p->current_instr >= p->num_instructions) {
                continue;
            }
            
            // Calculate laxity (deadline - remaining computation time)
            int laxity = p->remaining_deadline - p->remaining_computation_time;
            
            if (laxity < min_laxity) {
                min_laxity = laxity;
                min_process = i;
            }
        }
    }
    
    // Second pass: check for ties and apply LJF
    if (min_process != -1) {
        for (int i = 0; i < state->n; i++) {
            Process *p = &state->processes[i];
            
            // Skip processes that have completed or aren't in current round
            if (p->current_instr >= p->num_instructions) {
                continue;
            }
            
            if (last_serviced[i] == current_time && last_serviced[min_process] != current_time) {
                continue;
            }
            
            // Calculate laxity
            int laxity = p->remaining_deadline - p->remaining_computation_time;
            int min_proc_laxity = state->processes[min_process].remaining_deadline - 
                                  state->processes[min_process].remaining_computation_time;
            
            if (laxity == min_proc_laxity && i != min_process) {
                // Tie-breaking: choose process with longer remaining computation time (LJF)
                if (p->remaining_computation_time > state->processes[min_process].remaining_computation_time) {
                    min_process = i;
                }
            }
        }
    }
    
    return min_process;
}