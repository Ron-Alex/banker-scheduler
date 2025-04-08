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

// Scheduler types
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
    int *resource_vector;  // For REQUEST and RELEASE
    int resource_count;    // For USE_RESOURCES and REDUCE_RESOURCES
    int repeat_count;      // For USE_RESOURCES and REDUCE_RESOURCES
} Instruction;

// Resource structure
typedef struct {
    char *name;           // Resource type name (e.g., "hotel")
    char **instances;     // Instances of the resource (e.g., ["Hilton", "Marriott", ...])
    int count;            // Number of instances
    int *allocated;       // Track which process owns each instance (by process ID)
} Resource;

// Process structure
typedef struct {
    int id;                       // Process ID
    int initial_deadline;         // Original deadline
    int remaining_deadline;       // Current remaining deadline
    int initial_computation_time; // Original computation time
    int remaining_computation_time; // Current remaining computation time
    Instruction *instructions;    // Array of instructions
    int num_instructions;         // Number of instructions
    int current_instr;            // Current instruction index
    int *max;                     // Maximum resource demand
    int *allocation;              // Currently allocated resources
    int *need;                    // Remaining needed resources
    char *master_string;          // Current master string
    int pipe_read;                // Pipe for reading from parent
    int pipe_write;               // Pipe for writing to parent
    int missed_deadline;          // Flag for missed deadline
    pid_t pid;                    // Process PID
    int max_resources;            // Number of resource types for this process
} Process;

// System state structure
typedef struct {
    int m;                 // Number of resource types
    int n;                 // Number of processes
    Resource *resources;   // Array of resources
    Process *processes;    // Array of processes
    int *available;        // Available resources
    int *initial_available; // Initial available resources
} SystemState;

// Function prototypes for banker.c
void read_system_state(SystemState *state, char *filename);
void read_resource_instances(SystemState *state, char *filename);
void init_semaphores(SystemState *state);
int is_safe_state(SystemState *state);
int request_resources(SystemState *state, int process_id, int *request);
void release_resources(SystemState *state, int process_id, int *release);
void print_system_state(SystemState *state);

// Function prototypes for process.c
void create_processes(SystemState *state);
void execute_instruction(SystemState *state, int process_id);
void update_master_string(Process *process, Resource *resources, int m);
void use_resources(Process *process, Resource *resources, int m, int times);
void reduce_resources(Process *process, Resource *resources, int m, int reduction);

// Function prototypes for scheduler.c
int edf_scheduler(SystemState *state, int *last_serviced);
int llf_scheduler(SystemState *state, int *last_serviced);

// Function prototypes for utils.c
char* number_to_english(int num);
void parse_instructions(FILE *fp, Process *process);
void parse_resource_line(char *line, Resource *resource);
char* trim(char *str);
char** split_string(char *str, char *delim, int *count);

// Union for semaphores
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// External globals for IPC
extern int *semaphores;
extern int sem_count;
extern int shmid;

#endif // BANKER_H
