#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <cstring>
#include <cctype>
#include <numeric>
#include <limits> // Required for numeric_limits
#include <cmath>  // Required for std::min

/*
 * How to Compile and Run:
 * 1. Save the code as e.g., `banker_scheduler.cpp`.
 * 2. Compile using g++:
 *    g++ banker_scheduler.cpp -o banker_scheduler -std=c++11 -pthread
 * 3. Prepare your input files (e.g., `input.txt`, `resources.txt`).
 * 4. Run the program:
 *    ./banker_scheduler input.txt resources.txt
 */


// For semaphore operations
// Note: Some systems might require defining _GNU_SOURCE or similar for semun
#if defined(__linux__)
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf; // Required for SEM_INFO
};
#else
// Fallback for other systems (like macOS)
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
#endif

// Structure to hold process information
struct Process {
    int id;
    int deadline;
    int computationTime;
    std::vector<std::string> instructions;
    int currentInstruction;
    int remainingTime;
    std::vector<int> allocation;
    std::vector<int> maximum;
    std::vector<int> need;
    // Tracks specific instances held (e.g., "hotel" -> ["Hilton", "Marriott"])
    std::map<std::string, std::vector<std::string>> resourcesHeld;
    // Tracks counts for the master string display (e.g., "hotel" -> 3)
    std::map<std::string, int> masterStringCounts;
    std::string masterString;

    Process(int _id, int _deadline, int _computationTime)
        : id(_id), deadline(_deadline), computationTime(_computationTime),
          currentInstruction(0), remainingTime(_computationTime), masterString("empty") {}
};

// Structure to hold resource information
struct Resource {
    std::string name; // e.g., "hotel"
    std::vector<std::string> instances; // e.g., ["Hilton", "Marriott", ...]
};

// Function declarations
void readInputFile(const std::string& filename, int& m, int& n,
                  std::vector<int>& available,
                  std::vector<std::vector<int>>& maximum,
                  std::vector<Process>& processes);

void readResourcesFile(const std::string& filename, int m_expected, std::vector<Resource>& resources);
std::vector<std::string> tokenize(const std::string& str, char delimiter);
bool isSafe(const std::vector<Process>& processes, const std::vector<int>& available, int m, int n);
void updateMasterString(Process& process, const std::vector<Resource>& resources);
std::string pluralize(int count, const std::string& word);
void executeEDFScheduling(std::vector<Process> processes, std::vector<int> available,
                         std::vector<Resource> resources, int m, int n);
void executeLLFScheduling(std::vector<Process> processes, std::vector<int> available,
                         std::vector<Resource> resources, int m, int n);
void printSystemState(const std::vector<Process>& processes,
                     const std::vector<int>& available, int time, const std::vector<Resource>& resources);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <resources_file>" << std::endl;
        return 1;
    }

    int m, n; // m = number of resources, n = number of processes
    std::vector<int> available;
    std::vector<std::vector<int>> maximum;
    std::vector<Process> processes;
    std::vector<Resource> resources;

    // Read input file for processes and system state
    readInputFile(argv[1], m, n, available, maximum, processes);

    // Read resources file
    readResourcesFile(argv[2], m, resources); // Pass expected 'm' for validation

    // Initialize processes' allocation, need, and master string related fields
    for (int i = 0; i < n; ++i) {
        processes[i].allocation.resize(m, 0);
        processes[i].maximum = maximum[i]; // maximum was read for process i+1
        processes[i].need.resize(m);
        for(int j=0; j<m; ++j) {
            processes[i].need[j] = processes[i].maximum[j]; // Need = Max - Allocation (which is 0 initially)
        }
        // masterStringCounts map is initially empty
        // resourcesHeld map is initially empty
        processes[i].masterString = "empty"; // Initial master string
    }


    // Execute EDF scheduling
    std::cout << "======= EDF Scheduling with SJF Tie-Breaker =======" << std::endl;
    executeEDFScheduling(processes, available, resources, m, n);

    // Reset processes for LLF scheduling
    std::cout << "\nResetting system state for LLF Scheduling...\n" << std::endl;
    for (auto& process : processes) {
        process.allocation.assign(m, 0); // Reset allocation to all zeros
        process.need = process.maximum;    // Reset need to maximum
        process.currentInstruction = 0;
        process.remainingTime = process.computationTime;
        process.resourcesHeld.clear();
        process.masterStringCounts.clear(); // Reset counts
        process.masterString = "empty";     // Reset master string
    }

    // Execute LLF scheduling
    std::cout << "\n======= LLF Scheduling with LJF Tie-Breaker =======" << std::endl;
    executeLLFScheduling(processes, available, resources, m, n);

    return 0;
}

void readInputFile(const std::string& filename, int& m, int& n,
                  std::vector<int>& available,
                  std::vector<std::vector<int>>& maximum,
                  std::vector<Process>& processes) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open input file " << filename << std::endl;
        exit(1);
    }

    // Read m and n
    if (!(file >> m >> n)) {
        std::cerr << "Error: Failed to read m and n from " << filename << std::endl;
        exit(1);
    }

    // Read available resources
    available.resize(m);
    for (int i = 0; i < m; ++i) {
        if (!(file >> available[i])) {
             std::cerr << "Error: Failed to read available resource " << i << std::endl;
             exit(1);
        }
    }

    // Read maximum demands
    maximum.resize(n, std::vector<int>(m));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            if (!(file >> maximum[i][j])) {
                std::cerr << "Error: Failed to read maximum demand for process " << (i+1) << ", resource " << (j+1) << std::endl;
                exit(1);
            }
        }
    }

    // Skip any remaining whitespace or newlines before process definitions
    file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // Read process instructions
    for (int i = 0; i < n; ++i) {
        std::string line;
        // Find the start of the next process block
        while (std::getline(file, line)) {
             if (line.find("process_") != std::string::npos) {
                 break; // Found the start line
             }
        }
         if (file.eof() && line.find("process_") == std::string::npos) {
             std::cerr << "Error: Could not find definition for process " << (i+1) << std::endl;
             exit(1);
         }


        // Read deadline
        int deadline;
        if (!(file >> deadline)) {
             std::cerr << "Error: Failed to read deadline for process " << (i+1) << std::endl;
             exit(1);
        }


        // Read computation time
        int computationTime;
        if (!(file >> computationTime)) {
             std::cerr << "Error: Failed to read computation time for process " << (i+1) << std::endl;
             exit(1);
        }


        // Create a new process
        Process process(i + 1, deadline, computationTime);

        // Skip the rest of the computation time line
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        // Read instructions until "end."
        while (true) {
            if (!std::getline(file, line)) {
                 std::cerr << "Error: Unexpected end of file while reading instructions for process " << (i+1) << std::endl;
                 exit(1);
            }

            // Trim leading/trailing whitespace
            line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
            line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);

            if (line == "end.") {
                process.instructions.push_back("end.");
                break;
            }
            if (line.empty()) continue; // Skip empty lines

            // Remove trailing semicolon if present
            if (!line.empty() && line.back() == ';') {
                line.pop_back();
                // Trim trailing whitespace again after removing semicolon
                line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);
            }

            if (!line.empty()) {
                process.instructions.push_back(line);
            }
        }

        processes.push_back(process);
    }

    file.close();
}

void readResourcesFile(const std::string& filename, int m_expected, std::vector<Resource>& resources) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open resources file " << filename << std::endl;
        exit(1);
    }

    std::string line;
    int resource_index = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Parse resource line: "R1: hotel: Hilton, Marriott, Omni..."
        std::string::size_type pos1 = line.find(":");
        if (pos1 == std::string::npos) continue; // Skip lines without first colon

        std::string::size_type pos2 = line.find(":", pos1 + 1);
        if (pos2 == std::string::npos) continue; // Skip lines without second colon

        Resource resource;
        resource.name = line.substr(pos1 + 1, pos2 - pos1 - 1);
        // Trim whitespace from resource name
        resource.name.erase(0, resource.name.find_first_not_of(" \t"));
        resource.name.erase(resource.name.find_last_not_of(" \t") + 1);


        std::string instancesStr = line.substr(pos2 + 1);
        std::vector<std::string> instances = tokenize(instancesStr, ',');
        for (auto& instance : instances) {
            // Trim whitespace from instance name
            instance.erase(0, instance.find_first_not_of(" \t"));
            instance.erase(instance.find_last_not_of(" \t") + 1);
            if (!instance.empty()) {
                 resource.instances.push_back(instance);
            }
        }

        if (resource.instances.empty()) {
            std::cerr << "Warning: Resource type '" << resource.name << "' has no instances defined." << std::endl;
        }

        resources.push_back(resource);
        resource_index++;
    }

    file.close();

    // Validate against expected number of resources 'm'
     if (resources.size() != m_expected) {
         std::cerr << "Error: Mismatch between number of resources in input file (" << m_expected
                   << ") and resources file (" << resources.size() << ")" << std::endl;
         exit(1);
     }
}


std::vector<std::string> tokenize(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

// Banker's Algorithm: Check if the system is in a safe state
bool isSafe(const std::vector<Process>& processes, const std::vector<int>& available, int m, int n) {
    std::vector<int> work = available;
    std::vector<bool> finish(n, false);
    int num_processes = processes.size(); // Use actual number of processes passed

    // Temporary copy of process data needed for simulation
    std::vector<std::vector<int>> temp_need(num_processes, std::vector<int>(m));
    std::vector<std::vector<int>> temp_allocation(num_processes, std::vector<int>(m));
    for(int i=0; i<num_processes; ++i) {
        temp_need[i] = processes[i].need;
        temp_allocation[i] = processes[i].allocation;
    }


    int count = 0; // Count of finished processes
    while (count < num_processes) {
        bool found = false;
        for (int i = 0; i < num_processes; ++i) {
            // Check process i only if it's not finished
            if (!finish[i]) {
                bool canAllocate = true;
                // Check if Need_i <= Work
                for (int j = 0; j < m; ++j) {
                    if (temp_need[i][j] > work[j]) {
                        canAllocate = false;
                        break;
                    }
                }

                // If Need_i <= Work, simulate allocation
                if (canAllocate) {
                    // Work = Work + Allocation_i
                    for (int j = 0; j < m; ++j) {
                        work[j] += temp_allocation[i][j];
                    }
                    finish[i] = true; // Mark process i as finished
                    found = true;
                    count++;
                }
            }
        }
        // If no process could be allocated in this pass, the system is unsafe
        if (!found) {
            return false;
        }
    }

    // If all processes are finished, the state is safe
    return true;
}


// Generates the plural form of a word based on count
std::string pluralize(int count, const std::string& word) {
    std::string numStr;
    if (count == 1) numStr = "one";
    else if (count == 2) numStr = "two";
    else if (count == 3) numStr = "three";
    else if (count == 4) numStr = "four";
    else if (count == 5) numStr = "five";
    else numStr = std::to_string(count);

    if (count == 1) {
        return numStr + " " + word;
    } else {
        // Simple pluralization: add 's'. Assumes 'word' is singular.
        return numStr + " " + word + "s";
    }
}


// Updates the master string based on current masterStringCounts
void updateMasterString(Process& process, const std::vector<Resource>& resources) {
    process.masterString = "";
    std::map<std::string, int> tempCounts;

    // Populate tempCounts from masterStringCounts for currently held resource types with count > 0
    for (int i = 0; i < process.allocation.size(); ++i) {
        if (process.allocation[i] > 0) { // Only consider resources currently held
            const std::string& resourceName = resources[i].name;
            if (process.masterStringCounts.count(resourceName)) {
                 int count = process.masterStringCounts[resourceName];
                 if (count > 0) { // Only include if count is positive
                    tempCounts[resourceName] = count;
                 }
            }
            // If resource is held but not in masterStringCounts, it means use_resources hasn't been called for it yet.
            // Its count is effectively 0 for the master string until use_resources is called.
        }
    }

    // Convert to vector for sorting by resource name
    std::vector<std::pair<std::string, int>> sortedCounts(tempCounts.begin(), tempCounts.end());
    std::sort(sortedCounts.begin(), sortedCounts.end()); // Sorts by key (resource name)

    // Build the master string
    bool first = true;
    for (const auto& pair : sortedCounts) {
        if (!first) {
            process.masterString += ", ";
        }
        process.masterString += pluralize(pair.second, pair.first);
        first = false;
    }

    if (process.masterString.empty()) {
        process.masterString = "empty";
    }
}


// Prints the current state of the system
void printSystemState(const std::vector<Process>& processes,
                     const std::vector<int>& available, int time, const std::vector<Resource>& resources) {
    std::cout << "\n----------------------------------------\n";
    std::cout << "System State at time " << time << ":" << std::endl;
    std::cout << "  Available: [";
    for (size_t i = 0; i < available.size(); ++i) {
        std::cout << available[i] << (i == available.size() - 1 ? "" : ", ");
    }
    std::cout << "] (" << resources.size() << " resource types)" << std::endl;

    std::cout << "  Processes (" << processes.size() << " total):" << std::endl;
    for (const auto& p : processes) {
        std::cout << "    Process " << p.id << ":" << std::endl;
        std::cout << "      Deadline: " << p.deadline << ", Remaining Comp Time: " << p.remainingTime << std::endl;
        std::cout << "      Maximum:    [";
        for (size_t i = 0; i < p.maximum.size(); ++i) {
             std::cout << p.maximum[i] << (i == p.maximum.size() - 1 ? "" : ", ");
        }
        std::cout << "]" << std::endl;
        std::cout << "      Allocation: [";
        for (size_t i = 0; i < p.allocation.size(); ++i) {
            std::cout << p.allocation[i] << (i == p.allocation.size() - 1 ? "" : ", ");
        }
        std::cout << "]" << std::endl;

        std::cout << "      Need:       [";
        for (size_t i = 0; i < p.need.size(); ++i) {
            std::cout << p.need[i] << (i == p.need.size() - 1 ? "" : ", ");
        }
        std::cout << "]" << std::endl;

        std::cout << "      Held Instances: {";
        bool first_res = true;
        for(const auto& pair : p.resourcesHeld) {
            if (!pair.second.empty()) { // Only print if instances are held for this type
                if (!first_res) std::cout << "; ";
                std::cout << pair.first << ": [";
                bool first_inst = true;
                for(const auto& inst : pair.second) {
                    if (!first_inst) std::cout << ", ";
                    std::cout << inst;
                    first_inst = false;
                }
                std::cout << "]";
                first_res = false;
            }
        }
        if (first_res) std::cout << "none"; // If no resources held
        std::cout << "}" << std::endl;

         std::cout << "      Master String Counts: {";
        first_res = true;
        // Sort counts by resource name for consistent printing
        std::map<std::string, int> sortedMasterCounts(p.masterStringCounts.begin(), p.masterStringCounts.end());
        for(const auto& pair : sortedMasterCounts) {
             if (pair.second > 0) { // Only print if count > 0
                if (!first_res) std::cout << "; ";
                std::cout << pair.first << ": " << pair.second;
                first_res = false;
             }
        }
         if (first_res) std::cout << "none"; // If no counts > 0
        std::cout << "}" << std::endl;
         std::cout << "      Master String: \"" << p.masterString << "\"" << std::endl;
         std::cout << "      Next Instr: " << (p.currentInstruction < p.instructions.size() ? p.instructions[p.currentInstruction] : "N/A") << std::endl;
    }
    std::cout << "----------------------------------------" << std::endl;
}


// Executes the simulation using EDF scheduling with SJF tie-breaker
void executeEDFScheduling(std::vector<Process> processes, std::vector<int> available,
                         std::vector<Resource> resources, int m, int n) {
    int time = 0;
    std::vector<bool> completed(n, false);
    std::vector<bool> deadlineMissed(n, false);
    std::vector<int> semIds(m);
    int completedCount = 0;

    // Initialize semaphores for resource instances
    for (int i = 0; i < m; ++i) {
        key_t key = ftok(".", i + 1); // Unique key per resource type
        if (key == -1) {
            perror("ftok failed"); exit(1);
        }
        // Get semaphore set for the instances of this resource type
        semIds[i] = semget(key, resources[i].instances.size(), IPC_CREAT | IPC_EXCL | 0666);
        if (semIds[i] == -1) {
            if (errno == EEXIST) { // Semaphore set might already exist, try getting it
                semIds[i] = semget(key, resources[i].instances.size(), 0666);
                 if (semIds[i] == -1) {
                    perror("semget (get existing) failed"); exit(1);
                 }
                 // Optionally reset existing semaphores if needed, or assume they are clean
                 std::cout << "Warning: Reusing existing semaphore set for resource " << resources[i].name << std::endl;
            } else {
                perror("semget (create new) failed"); exit(1);
            }
        }

        // Initialize each semaphore in the set to 1 (available)
        union semun arg;
        arg.val = 1;
        for (size_t j = 0; j < resources[i].instances.size(); ++j) {
            if (semctl(semIds[i], j, SETVAL, arg) == -1) {
                perror("semctl SETVAL failed"); exit(1);
            }
        }
        std::cout << "Semaphore set created/initialized for " << resources[i].name
                  << " (ID: " << semIds[i] << ", Size: " << resources[i].instances.size() << ")" << std::endl;
    }

    // Main scheduling loop
    while (completedCount < n) {
        // Find process with earliest deadline (EDF)
        int selectedProcess = -1;
        int earliestDeadline = std::numeric_limits<int>::max();
        int shortestJobTime = std::numeric_limits<int>::max(); // For SJF tie-breaker

        bool runnableProcessExists = false;
        for (int i = 0; i < n; ++i) {
            if (!completed[i]) { // Only consider non-completed processes
                 runnableProcessExists = true;
                 int currentDeadline = processes[i].deadline; // Absolute deadline

                 // Check for deadline miss *before* selection
                 if (time >= processes[i].deadline && !deadlineMissed[i]) {
                     std::cout << "!!! Time " << time << ": Process " << (i + 1)
                               << " has missed its deadline (" << processes[i].deadline << ") !!!" << std::endl;
                     deadlineMissed[i] = true;
                     // Continue processing even if deadline missed
                 }

                 // EDF selection logic
                 if (currentDeadline < earliestDeadline) {
                     earliestDeadline = currentDeadline;
                     shortestJobTime = processes[i].remainingTime;
                     selectedProcess = i;
                 } else if (currentDeadline == earliestDeadline) {
                     // SJF Tie-breaker (shortest remaining computation time)
                     if (processes[i].remainingTime < shortestJobTime) {
                         shortestJobTime = processes[i].remainingTime;
                         selectedProcess = i;
                     }
                 }
            }
        }

        if (selectedProcess == -1) {
             if (!runnableProcessExists) {
                std::cout << "Time " << time << ": All processes completed." << std::endl;
                break; // Exit loop if all processes are done
             } else {
                 // This case might happen if all remaining processes are blocked (e.g., waiting for resources
                 // indefinitely in an unsafe state or a deadlock not caught by Banker's - unlikely with Banker's).
                 // Or simply, if time needs to advance for resources to become free.
                 std::cout << "Time " << time << ": No runnable process found eligible by EDF/SJF (possibly blocked). Advancing time." << std::endl;
                 time++;
                 // Re-evaluate deadlines based on new time in the next iteration
                 continue;
             }
        }


        Process& process = processes[selectedProcess];
        if (process.currentInstruction >= process.instructions.size()) {
            // Should not happen if completion logic is correct, but safeguard.
            std::cout << "Warning: Selected process " << process.id << " has no more instructions but isn't marked completed." << std::endl;
            completed[selectedProcess] = true;
            completedCount++;
            continue;
        }
        std::string instruction = process.instructions[process.currentInstruction];

        std::cout << "\nTime " << time << ": Selecting Process " << process.id
                  << " (Deadline: " << process.deadline << ", Remaining: " << process.remainingTime
                  << ") - Executing instruction: \"" << instruction << "\"" << std::endl;

        // --- Instruction Execution ---
        bool advancedTime = false; // Flag to track if time was advanced by the instruction

        if (instruction.find("compute") == 0) {
            int computeTime = 0;
            try {
                computeTime = std::stoi(instruction.substr(instruction.find_first_of("0123456789")));
            } catch (const std::invalid_argument& ia) {
                 std::cerr << "Error parsing compute time in instruction: " << instruction << std::endl;
                 computeTime = 1; // Default to 1 time unit on error? Or skip? Let's default.
            } catch (const std::out_of_range& oor) {
                 std::cerr << "Error: Compute time out of range in instruction: " << instruction << std::endl;
                 computeTime = 1;
            }

            std::cout << "  Action: Computing for " << computeTime << " time units." << std::endl;
            time += computeTime;
            process.remainingTime -= computeTime;
            process.currentInstruction++;
            advancedTime = true;
        }
        else if (instruction.find("request") == 0) {
            std::vector<int> request(m, 0);
            std::istringstream iss(instruction.substr(instruction.find_first_of("0123456789")));
            bool parseError = false;
            for (int i = 0; i < m; ++i) {
                if (!(iss >> request[i])) {
                    std::cerr << "Error parsing request vector in instruction: " << instruction << std::endl;
                    parseError = true;
                    break;
                }
            }

            if (parseError) {
                 process.currentInstruction++; // Skip malformed instruction
                 time++; // Consume 1 time unit for the failed attempt
                 process.remainingTime--;
                 advancedTime = true;
            } else {
                // 1. Check if Request_i <= Need_i
                bool validRequest = true;
                for (int i = 0; i < m; ++i) {
                    if (request[i] < 0 || request[i] > process.need[i]) {
                        validRequest = false;
                        std::cout << "  Error: Process " << process.id << " request [" << i << "]=" << request[i]
                                  << " exceeds its need [" << i << "]=" << process.need[i] << " or is negative." << std::endl;
                        break;
                    }
                }

                if (!validRequest) {
                    std::cout << "  Result: Request denied (exceeds need or invalid)." << std::endl;
                    process.currentInstruction++; // Move to next instruction
                    time++; // Consume 1 time unit
                    process.remainingTime--;
                    advancedTime = true;
                } else {
                    // 2. Check if Request_i <= Available
                    bool resourcesAvailable = true;
                    for (int i = 0; i < m; ++i) {
                        if (request[i] > available[i]) {
                            resourcesAvailable = false;
                            break;
                        }
                    }

                    if (!resourcesAvailable) {
                        std::cout << "  Result: Process " << process.id << " must wait (resources not available)." << std::endl;
                        // Process blocks, does not advance instruction, time advances
                        time++;
                        advancedTime = true;
                        // No change in remainingTime as no work was done by the process itself
                        // Wait for resources to be released by others
                    } else {
                        // 3. Simulate allocation and check safety (Banker's Algorithm)
                        std::vector<int> tempAvailable = available;
                        std::vector<int> tempAllocation = process.allocation;
                        std::vector<int> tempNeed = process.need;

                        // Pretend to allocate
                        for (int i = 0; i < m; ++i) {
                            tempAvailable[i] -= request[i];
                            tempAllocation[i] += request[i];
                            tempNeed[i] -= request[i];
                        }

                        // Check if the resulting state is safe
                        std::vector<Process> tempProcesses = processes; // Copy current state
                        tempProcesses[selectedProcess].allocation = tempAllocation;
                        tempProcesses[selectedProcess].need = tempNeed;

                        if (isSafe(tempProcesses, tempAvailable, m, n)) {
                            std::cout << "  Result: Request granted (safe state)." << std::endl;
                            // Grant the request - Update actual state
                            available = tempAvailable;
                            process.allocation = tempAllocation;
                            process.need = tempNeed;

                            // Allocate specific resource instances using semaphores
                            for (int i = 0; i < m; ++i) { // For each resource type
                                for (int j = 0; j < request[i]; ++j) { // For number requested of this type
                                    bool instanceAcquired = false;
                                    for (size_t k = 0; k < resources[i].instances.size(); ++k) { // Try each instance semaphore
                                        struct sembuf sb = {static_cast<unsigned short>(k), -1, IPC_NOWAIT}; // Try to decrement (acquire lock)
                                        if (semop(semIds[i], &sb, 1) == 0) {
                                            // Successfully acquired instance k
                                            process.resourcesHeld[resources[i].name].push_back(resources[i].instances[k]);
                                            std::cout << "    - Acquired instance: " << resources[i].instances[k] << " of type " << resources[i].name << std::endl;
                                            instanceAcquired = true;
                                            break; // Move to requesting the next instance of this type
                                        } else {
                                            if (errno != EAGAIN) { // EAGAIN means semaphore was 0 (busy), not an error
                                                perror("semop acquire failed unexpectedly");
                                                // Handle unexpected semaphore error? Exit or log heavily.
                                                exit(1); // Critical error
                                            }
                                        }
                                    }
                                     if (!instanceAcquired) {
                                        // This should NOT happen if Banker's check passed (available >= request)
                                        // and semaphores correctly reflect availability.
                                        std::cerr << "FATAL ERROR: Could not acquire instance for resource " << resources[i].name
                                                  << " despite Banker's algorithm check! Semaphore state mismatch?" << std::endl;
                                        // Rollback? Exit?
                                        exit(1);
                                    }
                                }
                            }
                            // Initialize master string count for newly acquired types if needed
                             for(int i=0; i<m; ++i) {
                                 if (request[i] > 0 && process.masterStringCounts.find(resources[i].name) == process.masterStringCounts.end()) {
                                    process.masterStringCounts[resources[i].name] = 0; // Initialize count to 0
                                 }
                             }


                            process.currentInstruction++;
                            time++; // Consume 1 time unit for request
                            process.remainingTime--;
                            advancedTime = true;
                        } else {
                            std::cout << "  Result: Request denied (unsafe state would result)." << std::endl;
                            // Process blocks, does not advance instruction, time advances
                            time++;
                            advancedTime = true;
                            // Wait for resources to be released by others
                        }
                    }
                }
            }
        }
        else if (instruction.find("release") == 0) {
            std::vector<int> release(m, 0);
            std::istringstream iss(instruction.substr(instruction.find_first_of("0123456789")));
             bool parseError = false;
            for (int i = 0; i < m; ++i) {
                if (!(iss >> release[i])) {
                     std::cerr << "Error parsing release vector in instruction: " << instruction << std::endl;
                     parseError = true;
                     break;
                }
            }

            if(parseError) {
                 process.currentInstruction++;
                 time++;
                 process.remainingTime--;
                 advancedTime = true;
            } else {
                // Check if Release_i <= Allocation_i
                bool validRelease = true;
                for (int i = 0; i < m; ++i) {
                    if (release[i] < 0 || release[i] > process.allocation[i]) {
                        validRelease = false;
                         std::cout << "  Error: Process " << process.id << " cannot release [" << i << "]=" << release[i]
                                  << " resources (allocated=" << process.allocation[i] << ")." << std::endl;
                        break;
                    }
                }

                if (!validRelease) {
                     std::cout << "  Result: Invalid release ignored." << std::endl;
                } else {
                    std::cout << "  Action: Releasing resources." << std::endl;
                    // Release resources and update state
                    for (int i = 0; i < m; ++i) {
                        if (release[i] > 0) {
                            available[i] += release[i];
                            process.allocation[i] -= release[i];
                            // Need increases back towards maximum, but Need = Max - Allocation is simpler
                            process.need[i] += release[i]; // Or recalculate: process.need[i] = process.maximum[i] - process.allocation[i];

                            // Release specific resource instances using semaphores
                            auto& heldInstances = process.resourcesHeld[resources[i].name];
                             if (static_cast<int>(heldInstances.size()) < release[i]) {
                                 std::cerr << "Error: Process " << process.id << " trying to release " << release[i]
                                           << " instances of " << resources[i].name << " but only holds "
                                           << heldInstances.size() << " according to resourcesHeld map!" << std::endl;
                                 // Corrective action? Skip release for this type? Exit?
                                 // Let's try to release what we can according to the map
                                 release[i] = heldInstances.size();
                             }


                            for (int releasedCount = 0; releasedCount < release[i]; ++releasedCount) {
                                if (heldInstances.empty()) {
                                    std::cerr << "Error: Inconsistency during release of " << resources[i].name << ". Expected more instances." << std::endl;
                                    break; // Should not happen if release[i] <= allocation[i] and maps are consistent
                                }
                                // Release the last acquired instance of this type (LIFO for simplicity)
                                std::string instanceToRelease = heldInstances.back();
                                heldInstances.pop_back();

                                // Find the semaphore index for this instance
                                auto it = std::find(resources[i].instances.begin(),
                                                   resources[i].instances.end(),
                                                   instanceToRelease);
                                if (it != resources[i].instances.end()) {
                                    int index = std::distance(resources[i].instances.begin(), it);
                                    struct sembuf sb = {static_cast<unsigned short>(index), 1, 0}; // Increment (release lock)
                                    if (semop(semIds[i], &sb, 1) == -1) {
                                        perror("semop release failed");
                                        // Handle semaphore error?
                                        exit(1); // Critical error
                                    }
                                    std::cout << "    - Released instance: " << instanceToRelease << " of type " << resources[i].name << std::endl;
                                } else {
                                    std::cerr << "Error: Released instance '" << instanceToRelease << "' not found in global resource list for " << resources[i].name << "!" << std::endl;
                                    // This indicates a major inconsistency.
                                    exit(1);
                                }
                            }
                            // If allocation becomes zero, clear related maps for cleanliness
                            if (process.allocation[i] == 0) {
                                process.resourcesHeld.erase(resources[i].name);
                                process.masterStringCounts.erase(resources[i].name); // Remove count if fully released
                                updateMasterString(process, resources); // Update string immediately
                            }
                        }
                    }
                     std::cout << "  Result: Resources released." << std::endl;
                }
                process.currentInstruction++;
                time++; // Consume 1 time unit for release
                process.remainingTime--;
                advancedTime = true;
            }
        }
         else if (instruction.find("use_resources") == 0) {
             std::stringstream ss(instruction.substr(instruction.find_first_of(" \t")+1));
             int x = 0, y = 0;
             if (!(ss >> x >> y)) {
                 std::cerr << "Error parsing use_resources parameters: " << instruction << std::endl;
                 x = 1; y = 0; // Default values on error?
             }
             std::cout << "  Action: Using resources for " << x << " time units, incrementing counts by " << y << "." << std::endl;

             time += x;
             process.remainingTime -= x;

             // Update master string counts for *all* currently held resource types
             for(int i=0; i<m; ++i) {
                 if(process.allocation[i] > 0) { // If holding any of resource type i
                     const std::string& resourceName = resources[i].name;
                      process.masterStringCounts[resourceName] += y; // Increment count
                 }
             }

             updateMasterString(process, resources); // Recalculate master string
             std::cout << "    - Master string updated to: \"" << process.masterString << "\"" << std::endl;
             process.currentInstruction++;
             advancedTime = true;
         }
         else if (instruction.find("reduce_resources") == 0) {
             std::stringstream ss(instruction.substr(instruction.find_first_of(" \t")+1));
             int x = 0, y = 0;
              if (!(ss >> x >> y)) {
                 std::cerr << "Error parsing reduce_resources parameters: " << instruction << std::endl;
                 x = 1; y = 0; // Default values on error?
             }
             std::cout << "  Action: Reducing resources for " << x << " time units, decrementing counts by " << y << "." << std::endl;

             time += x;
             process.remainingTime -= x;

              // Reduce master string counts for *all* currently held resource types
             for(int i=0; i<m; ++i) {
                 if(process.allocation[i] > 0) { // If holding any of resource type i
                     const std::string& resourceName = resources[i].name;
                     if (process.masterStringCounts.count(resourceName)) {
                         int currentCount = process.masterStringCounts[resourceName];
                         if (currentCount <= y) {
                              if (currentCount > 0) { // Only set to 1 if it was > 0 before
                                  process.masterStringCounts[resourceName] = 1; // Keep one instance
                              } else {
                                   process.masterStringCounts[resourceName] = 0; // If it was already 0, keep it 0
                              }
                         } else {
                             process.masterStringCounts[resourceName] -= y; // Reduce count
                         }
                     }
                     // If resourceName is not in masterStringCounts, its count is effectively 0, do nothing.
                 }
             }

             updateMasterString(process, resources); // Recalculate master string
             std::cout << "    - Master string updated to: \"" << process.masterString << "\"" << std::endl;
             process.currentInstruction++;
             advancedTime = true;
         }
         else if (instruction.find("print_resources_used") == 0) {
            std::cout << "  Action: Printing master string." << std::endl;
            std::cout << "    Process " << process.id << " Master String: \"" << process.masterString << "\"" << std::endl;

            process.currentInstruction++;
            time++; // Consume 1 time unit for print
            process.remainingTime--;
            advancedTime = true;
        }
        else if (instruction == "end.") {
             std::cout << "  Action: Process " << process.id << " terminating." << std::endl;
            // Release all remaining resources
            std::cout << "    - Releasing all held resources..." << std::endl;
            bool releasedAny = false;
            for (int i = 0; i < m; ++i) {
                int amountToRelease = process.allocation[i];
                if (amountToRelease > 0) {
                    available[i] += amountToRelease;
                    process.allocation[i] = 0;
                    process.need[i] = process.maximum[i]; // Reset need

                    auto& heldInstances = process.resourcesHeld[resources[i].name];
                     if (static_cast<int>(heldInstances.size()) != amountToRelease) {
                         std::cerr << "Warning: Mismatch at termination for P" << process.id << ", Res " << resources[i].name
                                   << ". Allocation=" << amountToRelease << ", HeldMapSize=" << heldInstances.size() << std::endl;
                         // Attempt to release based on heldInstances size
                         amountToRelease = heldInstances.size();
                     }

                    for (int releasedCount = 0; releasedCount < amountToRelease; ++releasedCount) {
                         if (heldInstances.empty()) break; // Should not happen
                         std::string instanceToRelease = heldInstances.back();
                         heldInstances.pop_back();

                         auto it = std::find(resources[i].instances.begin(), resources[i].instances.end(), instanceToRelease);
                         if (it != resources[i].instances.end()) {
                             int index = std::distance(resources[i].instances.begin(), it);
                             struct sembuf sb = {static_cast<unsigned short>(index), 1, 0};
                             if (semop(semIds[i], &sb, 1) == -1) {
                                 perror("semop release at end failed"); exit(1);
                             }
                             std::cout << "      - Released instance: " << instanceToRelease << std::endl;
                             releasedAny = true;
                         } else {
                              std::cerr << "Error: Instance '" << instanceToRelease << "' not found at termination release!" << std::endl; exit(1);
                         }
                    }
                    process.resourcesHeld.erase(resources[i].name);
                    process.masterStringCounts.erase(resources[i].name);
                }
            }
             if (!releasedAny) std::cout << "    - No resources were held." << std::endl;

             process.masterString = "empty"; // Ensure final string is empty
             process.remainingTime = 0; // Ensure remaining time is 0
             completed[selectedProcess] = true;
             completedCount++;
             std::cout << "  Result: Process " << process.id << " completed execution at time " << time << "." << std::endl;
             // No time cost for 'end.' itself
        }
        else {
             std::cerr << "Error: Unknown instruction for Process " << process.id << ": " << instruction << std::endl;
             process.currentInstruction++; // Skip unknown instruction
             time++; // Assume 1 time unit cost for unknown instruction?
             process.remainingTime--;
             advancedTime = true;
        }

        // Print system state after the instruction (if time potentially advanced or state changed)
        // Only print if not the 'end.' instruction which already prints completion message.
        if (instruction != "end.") {
             printSystemState(processes, available, time, resources);
        }

        // If time didn't advance (e.g., blocked request), ensure loop progresses
        if (!advancedTime) {
            // This case should ideally be handled within the instruction logic (e.g., request blocked -> time++)
            // Adding a safeguard here might mask logic errors. Let's assume instructions handle time advance.
            // std::cout << "Warning: Time did not advance in this step." << std::endl; // Debugging line
        }

        // Check if remaining time became negative (should not ideally happen if inputs are valid)
        if (process.remainingTime < 0 && !completed[selectedProcess]) {
            std::cout << "Warning: Process " << process.id << " has negative remaining time (" << process.remainingTime << ")." << std::endl;
        }

    } // End while loop (completedCount < n)


    // Final system state and summary
    std::cout << "\n======= EDF Simulation Complete =======" << std::endl;
    printSystemState(processes, available, time, resources);

    int missedCount = 0;
    std::cout << "Deadline Misses:" << std::endl;
    for(int i=0; i<n; ++i) {
        if (deadlineMissed[i]) {
            std::cout << "  - Process " << (i+1) << " missed its deadline." << std::endl;
            missedCount++;
        }
         if (!completed[i]) {
            std::cout << "  - Process " << (i+1) << " DID NOT COMPLETE." << std::endl;
         }
    }
     if (missedCount == 0 && completedCount == n) {
         std::cout << "  No deadlines missed and all processes completed." << std::endl;
     } else if (missedCount == 0 && completedCount < n) {
          std::cout << "  No deadlines missed, but " << (n - completedCount) << " processes did not complete (possible deadlock?)." << std::endl;
     }else {
         std::cout << "Total deadlines missed: " << missedCount << std::endl;
     }


    // Clean up semaphores
    std::cout << "Cleaning up semaphores..." << std::endl;
    for (int i = 0; i < m; ++i) {
        if (semctl(semIds[i], 0, IPC_RMID) == -1) {
            perror("semctl IPC_RMID failed");
            // Continue cleanup attempt
        } else {
            std::cout << "  Semaphore set ID " << semIds[i] << " removed." << std::endl;
        }
    }
}


// Executes the simulation using LLF scheduling with LJF tie-breaker
void executeLLFScheduling(std::vector<Process> processes, std::vector<int> available,
                          std::vector<Resource> resources, int m, int n) {
    int time = 0;
    std::vector<bool> completed(n, false);
    std::vector<bool> deadlineMissed(n, false);
    std::vector<int> semIds(m);
    int completedCount = 0;

    // Initialize semaphores (use different keys than EDF to avoid conflict)
    std::cout << "Initializing semaphores for LLF..." << std::endl;
    for (int i = 0; i < m; ++i) {
        key_t key = ftok(".", i + 101); // Use keys offset from EDF's
         if (key == -1) {
            perror("ftok failed"); exit(1);
        }
        semIds[i] = semget(key, resources[i].instances.size(), IPC_CREAT | IPC_EXCL | 0666);
        if (semIds[i] == -1) {
            if (errno == EEXIST) {
                 semIds[i] = semget(key, resources[i].instances.size(), 0666);
                 if (semIds[i] == -1) {
                    perror("semget (get existing) failed"); exit(1);
                 }
                 std::cout << "Warning: Reusing existing semaphore set for resource " << resources[i].name << std::endl;
            } else {
                perror("semget (create new) failed"); exit(1);
            }
        }

        union semun arg;
        arg.val = 1;
        for (size_t j = 0; j < resources[i].instances.size(); ++j) {
             if (semctl(semIds[i], j, SETVAL, arg) == -1) {
                perror("semctl SETVAL failed"); exit(1);
            }
        }
        std::cout << "Semaphore set created/initialized for " << resources[i].name
                  << " (ID: " << semIds[i] << ", Size: " << resources[i].instances.size() << ")" << std::endl;
    }

    // Main scheduling loop
     while (completedCount < n) {
        // Find process with least laxity (LLF)
        int selectedProcess = -1;
        int leastLaxity = std::numeric_limits<int>::max();
        int longestJobTime = -1; // For LJF tie-breaker

        bool runnableProcessExists = false;
        for (int i = 0; i < n; ++i) {
             if (!completed[i]) { // Only consider non-completed processes
                 runnableProcessExists = true;
                 int remainingDeadlineTime = processes[i].deadline - time; // Time until deadline
                 int laxity = remainingDeadlineTime - processes[i].remainingTime;

                 // Check for deadline miss *before* selection
                  if (remainingDeadlineTime < 0 && !deadlineMissed[i]) {
                     std::cout << "!!! Time " << time << ": Process " << (i + 1)
                               << " has missed its deadline (" << processes[i].deadline << ") !!!" << std::endl;
                     deadlineMissed[i] = true;
                 }

                 // LLF selection logic
                 if (laxity < leastLaxity) {
                     leastLaxity = laxity;
                     longestJobTime = processes[i].remainingTime;
                     selectedProcess = i;
                 } else if (laxity == leastLaxity) {
                     // LJF Tie-breaker (longest remaining computation time)
                     if (processes[i].remainingTime > longestJobTime) {
                         longestJobTime = processes[i].remainingTime;
                         selectedProcess = i;
                     }
                 }
             }
        }


        if (selectedProcess == -1) {
              if (!runnableProcessExists) {
                std::cout << "Time " << time << ": All processes completed." << std::endl;
                break; // Exit loop if all processes are done
             } else {
                 std::cout << "Time " << time << ": No runnable process found eligible by LLF/LJF (possibly blocked). Advancing time." << std::endl;
                 time++;
                 continue;
             }
        }

        Process& process = processes[selectedProcess];
        if (process.currentInstruction >= process.instructions.size()) {
            std::cout << "Warning: Selected process " << process.id << " has no more instructions but isn't marked completed." << std::endl;
            completed[selectedProcess] = true;
            completedCount++;
            continue;
        }
        std::string instruction = process.instructions[process.currentInstruction];

        std::cout << "\nTime " << time << ": Selecting Process " << process.id
                  << " (Laxity: " << (processes[selectedProcess].deadline - time - processes[selectedProcess].remainingTime)
                  << ", Remaining: " << process.remainingTime
                  << ") - Executing instruction: \"" << instruction << "\"" << std::endl;

        // --- Instruction Execution --- (Copied and adapted from EDF)
         bool advancedTime = false; // Flag to track if time was advanced by the instruction

        if (instruction.find("compute") == 0) {
            int computeTime = 0;
            try {
                computeTime = std::stoi(instruction.substr(instruction.find_first_of("0123456789")));
            } catch (const std::invalid_argument& ia) {
                 std::cerr << "Error parsing compute time in instruction: " << instruction << std::endl; computeTime = 1;
            } catch (const std::out_of_range& oor) {
                 std::cerr << "Error: Compute time out of range in instruction: " << instruction << std::endl; computeTime = 1;
            }
            std::cout << "  Action: Computing for " << computeTime << " time units." << std::endl;
            time += computeTime;
            process.remainingTime -= computeTime;
            process.currentInstruction++;
            advancedTime = true;
        }
        else if (instruction.find("request") == 0) {
             std::vector<int> request(m, 0);
            std::istringstream iss(instruction.substr(instruction.find_first_of("0123456789")));
            bool parseError = false;
            for (int i = 0; i < m; ++i) {
                if (!(iss >> request[i])) { std::cerr << "Error parsing request vector: " << instruction << std::endl; parseError = true; break; }
            }
             if (parseError) { process.currentInstruction++; time++; process.remainingTime--; advancedTime = true; }
             else {
                 bool validRequest = true;
                 for (int i = 0; i < m; ++i) {
                     if (request[i] < 0 || request[i] > process.need[i]) {
                         validRequest = false; std::cout << "  Error: Request exceeds need or invalid." << std::endl; break;
                     }
                 }
                 if (!validRequest) { std::cout << "  Result: Request denied (exceeds need)." << std::endl; process.currentInstruction++; time++; process.remainingTime--; advancedTime = true; }
                 else {
                     bool resourcesAvailable = true;
                     for (int i = 0; i < m; ++i) { if (request[i] > available[i]) { resourcesAvailable = false; break; } }

                     if (!resourcesAvailable) { std::cout << "  Result: Process " << process.id << " must wait." << std::endl; time++; advancedTime = true; }
                     else {
                         std::vector<int> tempAvailable = available; std::vector<int> tempAllocation = process.allocation; std::vector<int> tempNeed = process.need;
                         for (int i = 0; i < m; ++i) { tempAvailable[i] -= request[i]; tempAllocation[i] += request[i]; tempNeed[i] -= request[i]; }
                         std::vector<Process> tempProcesses = processes; tempProcesses[selectedProcess].allocation = tempAllocation; tempProcesses[selectedProcess].need = tempNeed;

                         if (isSafe(tempProcesses, tempAvailable, m, n)) {
                             std::cout << "  Result: Request granted (safe state)." << std::endl;
                             available = tempAvailable; process.allocation = tempAllocation; process.need = tempNeed;
                              for (int i = 0; i < m; ++i) {
                                 for (int j = 0; j < request[i]; ++j) {
                                     bool instanceAcquired = false;
                                     for (size_t k = 0; k < resources[i].instances.size(); ++k) {
                                         struct sembuf sb = {static_cast<unsigned short>(k), -1, IPC_NOWAIT};
                                         if (semop(semIds[i], &sb, 1) == 0) {
                                             process.resourcesHeld[resources[i].name].push_back(resources[i].instances[k]);
                                             std::cout << "    - Acquired instance: " << resources[i].instances[k] << std::endl;
                                             instanceAcquired = true; break;
                                         } else if (errno != EAGAIN) { perror("semop acquire failed"); exit(1); }
                                     }
                                     if (!instanceAcquired) { std::cerr << "FATAL ERROR: Could not acquire instance for " << resources[i].name << std::endl; exit(1); }
                                 }
                                  if (request[i] > 0 && process.masterStringCounts.find(resources[i].name) == process.masterStringCounts.end()) {
                                    process.masterStringCounts[resources[i].name] = 0; // Initialize count to 0
                                 }
                             }
                             process.currentInstruction++; time++; process.remainingTime--; advancedTime = true;
                         } else { std::cout << "  Result: Request denied (unsafe state)." << std::endl; time++; advancedTime = true; }
                     }
                 }
             }
        }
        else if (instruction.find("release") == 0) {
            std::vector<int> release(m, 0);
            std::istringstream iss(instruction.substr(instruction.find_first_of("0123456789")));
             bool parseError = false;
            for (int i = 0; i < m; ++i) { if (!(iss >> release[i])) { std::cerr << "Error parsing release vector: " << instruction << std::endl; parseError = true; break; } }
             if(parseError) { process.currentInstruction++; time++; process.remainingTime--; advancedTime = true; }
             else {
                 bool validRelease = true;
                 for (int i = 0; i < m; ++i) { if (release[i] < 0 || release[i] > process.allocation[i]) { validRelease = false; std::cout << "  Error: Invalid release." << std::endl; break; } }
                 if (!validRelease) { std::cout << "  Result: Invalid release ignored." << std::endl; }
                 else {
                     std::cout << "  Action: Releasing resources." << std::endl;
                     for (int i = 0; i < m; ++i) {
                         if (release[i] > 0) {
                             available[i] += release[i]; process.allocation[i] -= release[i]; process.need[i] += release[i];
                             auto& heldInstances = process.resourcesHeld[resources[i].name];
                             if (static_cast<int>(heldInstances.size()) < release[i]) { std::cerr << "Warning: Release inconsistency for " << resources[i].name << std::endl; release[i] = heldInstances.size(); }
                             for (int releasedCount = 0; releasedCount < release[i]; ++releasedCount) {
                                 if (heldInstances.empty()) { std::cerr << "Error: Release inconsistency." << std::endl; break; }
                                 std::string instanceToRelease = heldInstances.back(); heldInstances.pop_back();
                                 auto it = std::find(resources[i].instances.begin(), resources[i].instances.end(), instanceToRelease);
                                 if (it != resources[i].instances.end()) {
                                     int index = std::distance(resources[i].instances.begin(), it);
                                     struct sembuf sb = {static_cast<unsigned short>(index), 1, 0};
                                     if (semop(semIds[i], &sb, 1) == -1) { perror("semop release failed"); exit(1); }
                                     std::cout << "    - Released instance: " << instanceToRelease << std::endl;
                                 } else { std::cerr << "Error: Released instance not found!" << std::endl; exit(1); }
                             }
                             if (process.allocation[i] == 0) { process.resourcesHeld.erase(resources[i].name); process.masterStringCounts.erase(resources[i].name); updateMasterString(process, resources); }
                         }
                     }
                     std::cout << "  Result: Resources released." << std::endl;
                 }
                 process.currentInstruction++; time++; process.remainingTime--; advancedTime = true;
             }
        }
         else if (instruction.find("use_resources") == 0) {
              std::stringstream ss(instruction.substr(instruction.find_first_of(" \t")+1)); int x = 0, y = 0;
              if (!(ss >> x >> y)) { std::cerr << "Error parsing use_resources params: " << instruction << std::endl; x = 1; y = 0; }
              std::cout << "  Action: Using resources for " << x << " time, incrementing counts by " << y << "." << std::endl;
              time += x; process.remainingTime -= x;
              for(int i=0; i<m; ++i) { if(process.allocation[i] > 0) { process.masterStringCounts[resources[i].name] += y; } }
              updateMasterString(process, resources);
              std::cout << "    - Master string updated to: \"" << process.masterString << "\"" << std::endl;
              process.currentInstruction++; advancedTime = true;
         }
         else if (instruction.find("reduce_resources") == 0) {
              std::stringstream ss(instruction.substr(instruction.find_first_of(" \t")+1)); int x = 0, y = 0;
              if (!(ss >> x >> y)) { std::cerr << "Error parsing reduce_resources params: " << instruction << std::endl; x = 1; y = 0; }
              std::cout << "  Action: Reducing resources for " << x << " time, decrementing counts by " << y << "." << std::endl;
              time += x; process.remainingTime -= x;
              for(int i=0; i<m; ++i) {
                  if(process.allocation[i] > 0) {
                      const std::string& resourceName = resources[i].name;
                      if (process.masterStringCounts.count(resourceName)) {
                          int currentCount = process.masterStringCounts[resourceName];
                          if (currentCount <= y) { process.masterStringCounts[resourceName] = (currentCount > 0 ? 1 : 0); }
                          else { process.masterStringCounts[resourceName] -= y; }
                      }
                  }
              }
              updateMasterString(process, resources);
              std::cout << "    - Master string updated to: \"" << process.masterString << "\"" << std::endl;
              process.currentInstruction++; advancedTime = true;
         }
         else if (instruction.find("print_resources_used") == 0) {
            std::cout << "  Action: Printing master string." << std::endl;
            std::cout << "    Process " << process.id << " Master String: \"" << process.masterString << "\"" << std::endl;
            process.currentInstruction++; time++; process.remainingTime--; advancedTime = true;
        }
        else if (instruction == "end.") {
             std::cout << "  Action: Process " << process.id << " terminating." << std::endl;
             std::cout << "    - Releasing all held resources..." << std::endl; bool releasedAny = false;
             for (int i = 0; i < m; ++i) {
                 int amountToRelease = process.allocation[i];
                 if (amountToRelease > 0) {
                     available[i] += amountToRelease; process.allocation[i] = 0; process.need[i] = process.maximum[i];
                     auto& heldInstances = process.resourcesHeld[resources[i].name];
                     if (static_cast<int>(heldInstances.size()) != amountToRelease) { std::cerr << "Warning: Termination release mismatch." << std::endl; amountToRelease = heldInstances.size(); }
                     for (int releasedCount = 0; releasedCount < amountToRelease; ++releasedCount) {
                         if (heldInstances.empty()) break;
                         std::string instanceToRelease = heldInstances.back(); heldInstances.pop_back();
                         auto it = std::find(resources[i].instances.begin(), resources[i].instances.end(), instanceToRelease);
                         if (it != resources[i].instances.end()) {
                             int index = std::distance(resources[i].instances.begin(), it);
                             struct sembuf sb = {static_cast<unsigned short>(index), 1, 0};
                             if (semop(semIds[i], &sb, 1) == -1) { perror("semop release at end failed"); exit(1); }
                              std::cout << "      - Released instance: " << instanceToRelease << std::endl; releasedAny = true;
                         } else { std::cerr << "Error: Instance not found at termination!" << std::endl; exit(1); }
                     }
                     process.resourcesHeld.erase(resources[i].name); process.masterStringCounts.erase(resources[i].name);
                 }
             }
              if (!releasedAny) std::cout << "    - No resources were held." << std::endl;
             process.masterString = "empty"; process.remainingTime = 0; completed[selectedProcess] = true; completedCount++;
             std::cout << "  Result: Process " << process.id << " completed execution at time " << time << "." << std::endl;
        }
        else {
             std::cerr << "Error: Unknown instruction for Process " << process.id << ": " << instruction << std::endl;
             process.currentInstruction++; time++; process.remainingTime--; advancedTime = true;
        }

        // Print system state
        if (instruction != "end.") {
            printSystemState(processes, available, time, resources);
        }
        if (!advancedTime) {
             // Safeguard as in EDF
        }
         if (process.remainingTime < 0 && !completed[selectedProcess]) {
            std::cout << "Warning: Process " << process.id << " has negative remaining time (" << process.remainingTime << ")." << std::endl;
        }

    } // End while loop (completedCount < n)

     // Final system state and summary
    std::cout << "\n======= LLF Simulation Complete =======" << std::endl;
    printSystemState(processes, available, time, resources);

    int missedCount = 0;
    std::cout << "Deadline Misses:" << std::endl;
    for(int i=0; i<n; ++i) {
        if (deadlineMissed[i]) {
            std::cout << "  - Process " << (i+1) << " missed its deadline." << std::endl;
            missedCount++;
        }
         if (!completed[i]) {
            std::cout << "  - Process " << (i+1) << " DID NOT COMPLETE." << std::endl;
         }
    }
     if (missedCount == 0 && completedCount == n) {
         std::cout << "  No deadlines missed and all processes completed." << std::endl;
     } else if (missedCount == 0 && completedCount < n) {
          std::cout << "  No deadlines missed, but " << (n - completedCount) << " processes did not complete (possible deadlock?)." << std::endl;
     }else {
         std::cout << "Total deadlines missed: " << missedCount << std::endl;
     }

    // Clean up semaphores
     std::cout << "Cleaning up semaphores..." << std::endl;
    for (int i = 0; i < m; ++i) {
         if (semctl(semIds[i], 0, IPC_RMID) == -1) {
            perror("semctl IPC_RMID failed");
        } else {
            std::cout << "  Semaphore set ID " << semIds[i] << " removed." << std::endl;
        }
    }
}