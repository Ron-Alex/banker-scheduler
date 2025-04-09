#ifndef BANKER_H
#define BANKER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <limits.h>

// Schedulers - EDF OR LLF
#define SCHEDULER_EDF 0
#define SCHEDULER_LLF 1

// Process signals
#define SIGNAL_EXECUTE 1
#define SIGNAL_TERMINATE 2
#define SIGNAL_GRANTED 3
#define SIGNAL_DENIED 4

// Instruction types
typedef enum {
    COMPUTE,
    REQUEST,
    USE_RESOURCES,
    REDUCE_RESOURCES,
    RELEASE,
    PRINT_RESOURCES,
    END
} InstructionType;

// Instruction structure
typedef struct {
    InstructionType type;
    int computation_time;
    int *resource_vector;  
    int resource_count;
    int repeat_count; 
} Instruction;

// Resources and instances
typedef struct {
    char *name;           
    char **instances;     
    int count;            
    int *allocated;
} Resource;

// Process structure
typedef struct {
    int id;                       // Process ID
    int initial_deadline;         // Starting value
    int remaining_deadline;       // Time left
    int initial_computation_time; // Computation needed at start
    int remaining_computation_time; // Remaning computation time
    Instruction *instructions;    // Array of instructions
    int num_instructions;         // Total instruction count
    int current_instr;            // Current index
    int *max;                     // Maximum resources that might be requested
    int *allocation;              // Currently allocated resources
    int *need;                    // Remaining resources needed
    char *master_string;          // Running string
    int pipe_read;                // Pipe for reading
    int pipe_write;               // Pipe for writing
    int missed_deadline;          // Flag for missed deadline
    pid_t pid;                    // Process PID
    int max_resources;            // Number of resource types
} Process;

// Holds all system info, processes, and resources
typedef struct {
    int m;                 // Total resource types
    int n;                 // Total processes
    Resource *resources;   // Resource info
    Process *processes;    // Process info
    int *available;        // Available resources
    int *initial_available; // Starting resources
} SystemState;

// Function prototypes for resource reading/initialization and Banker's algorithm
void read_system_state(SystemState *state, char *filename);
void read_resource_instances(SystemState *state, char *filename);
void init_semaphores(SystemState *state);
int is_safe_state(SystemState *state);
int request_resources(SystemState *state, int process_id, int *request);
void release_resources(SystemState *state, int process_id, int *release);
void print_system_state(SystemState *state);

// Function prototypes for process creation and execution
void create_processes(SystemState *state);
void execute_instruction(SystemState *state, int process_id);
void update_master_string(Process *process, Resource *resources, int m);
void use_resources(Process *process, Resource *resources, int m, int times);
void reduce_resources(Process *process, Resource *resources, int m, int reduction);

// Function prototypes to pick next process
int edf_scheduler(SystemState *state, int *last_serviced);
int llf_scheduler(SystemState *state, int *last_serviced);

// Function prototypes for utility - parsing, trimmming etc
char* number_to_english(int num);
void parse_instructions(FILE *fp, Process *process);
void parse_resource_line(char *line, Resource *resource);
char* trim(char *str);
char** split_string(char *str, char *delim, int *count);

// Union definition for semaphores
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Globals for semaphores, counts and memory ID
extern int *semaphores;
extern int sem_count;
extern int shmid;

#endif // BANKER_H
