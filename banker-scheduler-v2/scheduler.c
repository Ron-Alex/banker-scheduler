#include "banker.h"

// EDF Scheduler with SJF tie-breaking
int edf_scheduler(SystemState *state, int *last_serviced) {
    int min_deadline = INT_MAX;
    int min_process = -1;
    int current_time = 0;
    
    for (int i = 0; i < state->n; i++) {
        if (last_serviced[i] > current_time) {
            current_time = last_serviced[i];
        }
    }
    
    for (int i = 0; i < state->n; i++) {
        Process *p = &state->processes[i];
        
        if (p->current_instr >= p->num_instructions) {
            continue;
        }
        
        if (last_serviced[i] == current_time) {
            continue;
        }
        
        if (p->remaining_deadline < min_deadline) {
            min_deadline = p->remaining_deadline;
            min_process = i;
        }
    }
    
    if (min_process == -1) {
        for (int i = 0; i < state->n; i++) {
            Process *p = &state->processes[i];
            
            if (p->current_instr >= p->num_instructions) {
                continue;
            }
            
            if (p->remaining_deadline < min_deadline) {
                min_deadline = p->remaining_deadline;
                min_process = i;
            }
        }
    }
    
    if (min_process != -1) {
        for (int i = 0; i < state->n; i++) {
            Process *p = &state->processes[i];
            
            if (p->current_instr >= p->num_instructions) {
                continue;
            }
            
            if (last_serviced[i] == current_time && last_serviced[min_process] != current_time) {
                continue;
            }
            
            if (p->remaining_deadline == min_deadline && i != min_process) {
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
    int current_time = 0; 
    
    for (int i = 0; i < state->n; i++) {
        if (last_serviced[i] > current_time) {
            current_time = last_serviced[i];
        }
    }
    
    for (int i = 0; i < state->n; i++) {
        Process *p = &state->processes[i];
        
        if (p->current_instr >= p->num_instructions) {
            continue;
        }
        
        if (last_serviced[i] == current_time) {
            continue;
        }
        
        int laxity = p->remaining_deadline - p->remaining_computation_time;
        
        if (laxity < min_laxity) {
            min_laxity = laxity;
            min_process = i;
        }
    }
    
    if (min_process == -1) {
        for (int i = 0; i < state->n; i++) {
            Process *p = &state->processes[i];
            
            if (p->current_instr >= p->num_instructions) {
                continue;
            }
            
            int laxity = p->remaining_deadline - p->remaining_computation_time;
            
            if (laxity < min_laxity) {
                min_laxity = laxity;
                min_process = i;
            }
        }
    }
    
    if (min_process != -1) {
        for (int i = 0; i < state->n; i++) {
            Process *p = &state->processes[i];
            
            if (p->current_instr >= p->num_instructions) {
                continue;
            }
            
            if (last_serviced[i] == current_time && last_serviced[min_process] != current_time) {
                continue;
            }
            
            int laxity = p->remaining_deadline - p->remaining_computation_time;
            int min_proc_laxity = state->processes[min_process].remaining_deadline - 
                                  state->processes[min_process].remaining_computation_time;
            
            if (laxity == min_proc_laxity && i != min_process) {
                if (p->remaining_computation_time > state->processes[min_process].remaining_computation_time) {
                    min_process = i;
                }
            }
        }
    }
    
    return min_process;
}