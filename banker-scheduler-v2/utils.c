#include "banker.h"
#include <ctype.h>

// Convert a number to English words
char* number_to_english(int num) {
    static const char *ones[] = {"", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};
    static const char *teens[] = {"ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
    static const char *tens[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
    
    char *result = malloc(100);
    result[0] = '\0';
    
    if (num == 0) {
        strcpy(result, "zero");
        return result;
    }
    
    if (num < 0) {
        strcat(result, "negative ");
        num = -num;
    }
    
    if (num >= 100) {
        sprintf(result + strlen(result), "%s hundred ", ones[num / 100]);
        num %= 100;
        
        if (num > 0) {
            strcat(result, "and ");
        }
    }
    
    if (num >= 20) {
        strcat(result, tens[num / 10]);
        
        if (num % 10 > 0) {
            strcat(result, "-");
            strcat(result, ones[num % 10]);
        }
    } else if (num >= 10) {
        strcat(result, teens[num - 10]);
    } else if (num > 0) {
        strcat(result, ones[num]);
    }
    
    return result;
}

// Trim whitespace from a string
char* trim(char *str) {
    char *end;
    
    // Trim leading space
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) // All spaces?
        return str;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator
    *(end + 1) = 0;
    
    return str;
}

// Split a string by a delimiter
char** split_string(char *str, char *delim, int *count) {
    char *copy = strdup(str);
    char *token;
    char **result = NULL;
    *count = 0;
    
    token = strtok(copy, delim);
    while (token != NULL) {
        (*count)++;
        result = realloc(result, (*count) * sizeof(char*));
        result[(*count) - 1] = strdup(trim(token));
        token = strtok(NULL, delim);
    }
    
    free(copy);
    return result;
}

// Parse a line of resource instances
void parse_resource_line(char *line, Resource *resource) {
    // Format: "R1: hotel: Hilton, Marriott, Omni, ..."
    char *ptr = strchr(line, ':');
    if (!ptr) {
        fprintf(stderr, "Invalid resource line format: %s\n", line);
        exit(1);
    }
    
    ptr++; // Skip ':'
    char *name_end = strchr(ptr, ':');
    if (!name_end) {
        fprintf(stderr, "Invalid resource line format: %s\n", line);
        exit(1);
    }
    
    // Extract resource name
    *name_end = '\0';
    resource->name = strdup(trim(ptr));
    
    // Extract instances
    ptr = name_end + 1;
    int count;
    char **instances = split_string(ptr, ",", &count);
    
    resource->instances = instances;
    resource->count = count;
}
// Parse process instructions from a file
// Parse instructions from a file for a process
void parse_instructions(FILE *file, Process *process) {
    // Initialize the process
    process->num_instructions = 0;
    process->instructions = NULL;
    int max_resources = process->max_resources;
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        char *trimmed = trim(line);
        
        // Skip empty lines and comments
        if (trimmed[0] == '\0' || trimmed[0] == '#')
            continue;
            
        if (strncmp(trimmed, "compute", 7) == 0) {
            // Format: "compute X ;"
            int computation_time;
            if (sscanf(trimmed, "compute %d ;", &computation_time) != 1) {
                fprintf(stderr, "Invalid compute instruction: %s\n", trimmed);
                exit(1);
            }
            
            process->num_instructions++;
            process->instructions = realloc(process->instructions, 
                                           process->num_instructions * sizeof(Instruction));
            
            Instruction *instr = &process->instructions[process->num_instructions - 1];
            instr->type = COMPUTE;
            instr->computation_time = computation_time;
            
        } else if (strncmp(trimmed, "request", 7) == 0) {
            // Format: "request X Y Z ... ;"
            process->num_instructions++;
            process->instructions = realloc(process->instructions, 
                                           process->num_instructions * sizeof(Instruction));
            
            Instruction *instr = &process->instructions[process->num_instructions - 1];
            instr->type = REQUEST;
            instr->computation_time = 1;  // Fixed time for request
            
            // Parse resource vector
            instr->resource_vector = malloc(process->max_resources * sizeof(int));
            
            // Skip "request" keyword
            char *ptr = trimmed + 7;
            
            for (int i = 0; i < process->max_resources; i++) {
                while (*ptr && (*ptr == ' ' || *ptr == '\t'))
                    ptr++;
                
                if (*ptr == ';') {
                    fprintf(stderr, "Not enough values in request: %s\n", trimmed);
                    exit(1);
                }
                
                if (sscanf(ptr, "%d", &instr->resource_vector[i]) != 1) {
                    fprintf(stderr, "Invalid resource value in request: %s\n", trimmed);
                    exit(1);
                }
                
                // Move to next value
                while (*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != ';')
                    ptr++;
            }
            
        } else if (strncmp(trimmed, "use_resources", 13) == 0) {
            // Format: "use_resources X Y ;"
            int computation_time, repeat_count;
            if (sscanf(trimmed, "use_resources %d %d ;", &computation_time, &repeat_count) != 2) {
                fprintf(stderr, "Invalid use_resources instruction: %s\n", trimmed);
                exit(1);
            }
            
            process->num_instructions++;
            process->instructions = realloc(process->instructions, 
                                           process->num_instructions * sizeof(Instruction));
            
            Instruction *instr = &process->instructions[process->num_instructions - 1];
            instr->type = USE_RESOURCES;
            instr->computation_time = computation_time;
            instr->repeat_count = repeat_count;
            
        } else if (strncmp(trimmed, "reduce_resources", 16) == 0) {
            // Format: "reduce_resources X Y ;"
            int computation_time, reduction;
            if (sscanf(trimmed, "reduce_resources %d %d ;", &computation_time, &reduction) != 2) {
                fprintf(stderr, "Invalid reduce_resources instruction: %s\n", trimmed);
                exit(1);
            }
            
            process->num_instructions++;
            process->instructions = realloc(process->instructions, 
                                           process->num_instructions * sizeof(Instruction));
            
            Instruction *instr = &process->instructions[process->num_instructions - 1];
            instr->type = REDUCE_RESOURCES;
            instr->computation_time = computation_time;
            instr->repeat_count = reduction;
            
        } else if (strncmp(trimmed, "release", 7) == 0) {
            // Format: "release X Y Z ... ;"
            // Parse resource vector
            char *ptr = trimmed + 7; // Skip "release"
            char *semicolon = strchr(ptr, ';');
            if (!semicolon) {
                fprintf(stderr, "Missing semicolon in release: %s\n", trimmed);
                exit(1);
            }
            
            // Count how many resources are in this release
            int num_resources = 0;
            char *temp_ptr = ptr;
            while (temp_ptr < semicolon) {
                while (isspace(*temp_ptr)) temp_ptr++;
                if (temp_ptr >= semicolon) break;
                
                // Skip over the number
                while (temp_ptr < semicolon && !isspace(*temp_ptr) && *temp_ptr != ';') temp_ptr++;
                num_resources++;
            }
            
            if (num_resources > max_resources) {
                max_resources = num_resources;
            }
            
            process->num_instructions++;
            process->instructions = realloc(process->instructions, 
                                           process->num_instructions * sizeof(Instruction));
            
            Instruction *instr = &process->instructions[process->num_instructions - 1];
            instr->type = RELEASE;
            instr->computation_time = 1;  // Fixed time for release
            
            // Allocate and parse resource vector
            instr->resource_vector = malloc(max_resources * sizeof(int));
            
            // Parse the actual values
            temp_ptr = ptr;
            for (int i = 0; i < num_resources; i++) {
                while (isspace(*temp_ptr)) temp_ptr++;
                
                if (sscanf(temp_ptr, "%d", &instr->resource_vector[i]) != 1) {
                    fprintf(stderr, "Invalid resource value in release: %s\n", trimmed);
                    exit(1);
                }
                
                // Move to next value
                while (!isspace(*temp_ptr) && *temp_ptr != ';') temp_ptr++;
            }
            
        } else if (strncmp(trimmed, "print_resources_used", 20) == 0) {
            // Format: "print_resources_used ;"
            process->num_instructions++;
            process->instructions = realloc(process->instructions, 
                                           process->num_instructions * sizeof(Instruction));
            
            Instruction *instr = &process->instructions[process->num_instructions - 1];
            instr->type = PRINT_RESOURCES;
            instr->computation_time = 1;  // Fixed time for print
            
        } else {
            fprintf(stderr, "Unknown instruction: %s\n", trimmed);
            exit(1);
        }
    }
    
    // Set max_resources in the process structure
    process->max_resources = max_resources;
}