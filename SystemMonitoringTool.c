#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>
#include <utmp.h>
#include <errno.h>

type def struct memory {

    double total_memory;
    double used_memory;
    double total_virtual;
    double used_virtual;

} memory;

type def struct cpu_stats {
    
    long int user;
    long int nice;
    long int system;
    long int idle;
    long int iowait;
    long int irq;
    long int softirq;

} cpu_stats;

void signal_handler(int sig) {
    char ans;

    // If Ctrl-Z detected it will proceed as normal
    if (sig == SIGTSTP) { return; }

    // If Ctrl-C detected it will ask user if they want to proceed
    if (sig == SIGINT) {
        printf("\nCtrl-C detected: ");
        printf("Do you want to quit? (press 'y' if yes) ");

        // Wait for user input
        int ret = scanf(" %c", &ans);
        if (ret == EOF) {
            if (errno == EINTR) {
                printf("\nSignal detected during scanf, resuming...\n");
                return;
            } 
            else {
                perror("scanf error");
                exit(EXIT_FAILURE);
            }
        }

        if (ans == 'y' || ans == 'Y') {
            exit(EXIT_SUCCESS);
        } else {
            printf("Resuming...\n");
        }
    }
}

void headerUsage(int samples, int tdelay){
    /**
    * Retrieves and prints the current memory usage in kilobytes
    * 
    * @samples: number of samples 
    * @tdelay: time delay between each sample in seconds
    *
    * The function uses the getrusage function from the sys/resource.h library to retrieve information about the memory usage
    * of the calling process and then prints the result to the terminal.
    *
    * Outputs: Nbr of samples: [samples] -- every [tdelay] secs
    *           Memory usage: [used_memory] kilobytes
    */

    // Get information about the utilization of memory in kilobytes and store it in used_memory
    struct rusage usage;
    int result = getrusage(RUSAGE_SELF, &usage);

    // Check for errors in getrusage
    if (result != 0) {
        fprintf(stderr, "Error: getrusage failed with error code: %d - %s\n", errno, strerror(errno));
        return;
    }

    long used_memory = usage.ru_maxrss;

    // Print the number of samples, tdelay, and memory usage
    printf("Nbr of samples: %d -- every %d secs\nMemory usage: %ld kilobytes\n", samples, tdelay, used_memory);

}

void footerUsage(){
    /**
     * Retrives and prints system information
     *
     * The function gets information about the system using uname function from sys/utsname.h library
     * 
     * Output:
     * --------------------------------------------
     * ### System Information ###
     *  System Name = [sysname]
     *  Machine Name = [nodename]
     *  Version = [version]
     *  Release = [release]
     *  Architecture = [machine]
     *  --------------------------------------------
     *
     */
    
    // Retrive System information
    struct utsname sysinfo;
    int result = uname(&sysinfo);

    // Check for errors in uname
    if (result == -1) {
        fprintf(stderr, "Error: uname failed with error code: %d - %s\n", errno, strerror(errno));
        return;
    }

    // Prints relevant information such as system name, machine name, version, release, architecture
    printf("--------------------------------------------\n");
    printf("### System Information ###\n");
    printf(" System Name = %s\n", sysinfo.sysname);
    printf(" Machine Name = %s\n", sysinfo.nodename);
    printf(" Version = %s\n", sysinfo.version);
    printf(" Release = %s\n", sysinfo.release);
    printf(" Architecture = %s\n", sysinfo.machine);
    printf("--------------------------------------------\n");

}

void memoryGraphicsOutput(char memoryGraphics[1024], double memory_current, double* memory_previous, int i){
    /**
    * Outputs a graphical representation of the change in memory
    * 
    * @memoryGraphics: char array of size 1024 to store the output
    * @memory_current: double representing the current memory usage
    * @memory_previous: a pointer to a double representing the previous memory usage
    * @i: integer representing the index of current iteration
    *
    *
    */

    // If first iteration then store previous memory usage as current memeory to prevent seg fault
    if (i == 0){ *memory_previous = memory_current; }
        
    // Absolute value of difference in memory usage
    double diff = memory_current - *memory_previous;
    double abs_diff = fabs(diff);
    
    // initialize the stting for viuals and make the length add one for every 0.01 change in memory
    char visual[1024] = "   |";
    int visual_len = (int)( abs_diff / 0.01 );
    char last_char;
    char sign;

    // Chppse the appropriate sign and last character based on the difference in memory usage
    // Positive non zero difference makes sign '#' and last character '*'. positive zero changes the last char to 'o'
    // Negative difference makes the sign ':' and last character '@'
    if (diff >= 0) { 
        sign = '#';
        last_char = '*';
        if (visual_len == 0) { last_char = 'o'; }
    }
    else { sign = ':'; last_char = '@'; }

    // Set previous memory to current for the next iteration
    *memory_previous = memory_current;
    
    // Create the graphics starting at 4 to account for the starting of visuals ("   |")
    for (int i = 4; i < visual_len + 4; i++){
        visual[i] = sign;
    }
    visual[visual_len + 4] = '\0';
    
    strncat(visual , &last_char, 1);

    // Add the graphics to the output string
    sprintf(memoryGraphics, "%s %.2f (%.2f)", visual, abs_diff, memory_current);

}

void memoryStats(int pipefd[2]){

    // Define memory data type
    memory info;

    // Gets memory
    struct sysinfo memory;
    int result = sysinfo(&memory);

    // Check for errors in sysinfo
    if (result != 0) {
        fprintf(stderr, "Error: sysinfo failed with error code: %d - %s\n", errno, strerror(errno));
        return;
    }

    // Calculate the memory usgae and total memory in GB
    info.total_memory = (double) memory.totalram / (1024 * 1024 * 1024);
    info.used_memory =  (double) (memory.totalram - memory.freeram) / (1024 * 1024 * 1024);
    info.total_virtual = (double) (memory.totalram + memory.totalswap) / (1024 * 1024 * 1024);
    info.used_virtual = (double) (memory.totalram - memory.freeram + memory.totalswap - memory.freeswap) / (1024 * 1024 * 1024);

    ssize_t bytes_written = write(pipefd[1], &info, sizeof(info));

    if (bytes_written == -1) {
        perror("Error writing to pipe");
        return;
    }       

}

void systemOutput(char terminal[1024][1024], bool graphics, int i, double* memory_previous, memory info){
    /**
    * Function Gets memory information of function and stores it in terminal then prints all memory information thus far
    * Gets information using sysinfo function from sys/sysinfo.h library
    *
    * @terminal: array of strings for the output
    * @graphics: boolean value indicaing if graphics option has been selected
    * @i: int value indicating the current iteration
    * @memory_previous: pointer to double that contains the last memory usage calculated
    *
    */

    // Divider
    printf("--------------------------------------------\n");
    printf("### Memory ### (Phys.Used/Tot -- Virtual Used/Tot)\n");

    // Gets memory
    struct sysinfo memory;
    sysinfo (&memory);
    
    
    // Add the memmory usage to terminal
    sprintf(terminal[i], "%.2f GB / %.2f GB -- %.2f GB / %.2f GB", info.used_memory, info.total_memory, info.used_virtual, info.total_virtual);

    // If graphics is enabled then add the visual from the graphics function
    if (graphics){ 
        char graphics_output[1024]; memoryGraphicsOutput(graphics_output, info.used_memory, info.memory_previous, i);
        strcat(terminal[i], graphics_output); 
    }

    // Prints terminal
    for (int j = 0; j <= i; j++){
        printf("%s\n", terminal[j]);
    }
}

void userOutput(int pipefd[2]){
    /**
    * Prints information about current user sessions
    * gets information from utmp.h library and prints to each session to terminal
    *
    */

    printf("--------------------------------------------\n");
    printf("### Sessions/users ###\n");

    // Initialize and open utmp
    struct utmp *utmp;
    if (utmpname(_PATH_UTMP) == -1) {
        perror("Error setting utmp file");
        return;
    }

    setutent();

    while ((utmp = getutent()) != NULL) {
        // Check for errors in getutent() -------------------

        // Checks for user process
        if (utmp->ut_type == USER_PROCESS) {
            // Prints the User, session, host
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), "%s\t %s (%s)\n", utmp->ut_user, utmp->ut_line, utmp->ut_host);

            // Write the formatted string to the pipe
            ssize_t bytes_written = write(pipefd[1], buffer, strlen(buffer));
            if (bytes_written == -1) {
                perror("Error writing to pipe");
                return;
            }       
        }
    }

    // Check for errors in endutent()
    endutent();

    close(pipefd[1]);

}

void cpuStats(int pipefd[2]){

    cpu_stats info;
    // Gets system info
    struct sysinfo cpu;
    if (sysinfo(&cpu) != 0) {
        fprintf(stderr, "Error: failed to get system info. (%s)\n", strerror(errno));
        return;
    }
    
    // Opens proc stat file with cpu usage
    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: failed to open /proc/stat. (%s)\n", strerror(errno));
        return;
    }

    long int user, nice, system, idle, iowait, irq, softirq;
    int read_items = fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld", &info.user, &info.nice, &info.system, &info.idle, &info.iowait, &info.irq, &info.softirq);
    fclose(fp);

    // Checks that all the items have been read
    if (read_items != 7) {
        fprintf(stderr, "Error: failed to read CPU values from /proc/stat. Read %d items instead of 7.\n", read_items);
        return;
    }

    ssize_t bytes_written = write(pipefd[1], &info, sizeof(info));

    if (bytes_written == -1) {
        perror("Error writing to pipe");
        return;
    }  

}

void CPUGraphics(char terminal[1024][1024], double usage, int i){
    /**
    * Prints a graphical representation of CPU usage
    *
    * @terminal: array of strings that stores the output to print to the terminal
    * @usage: current cpu usage
    * @i: current iteration of program
    *
    * The graphical represntation for usage always starts with '|||' then adds another bar for every percent change in usage
    *
    */

    // Calculates the length of the visual string adding 12 for starting characters
    int visual_len = (int)(usage) + 12;

    // Creates graphics and stores in terminal
    strcpy(terminal[i], "         ");

    for (int j = 9; j < visual_len; j++){
        terminal[i][j] = '|';
    }
    terminal[i][visual_len] = '\0';

    // Adds the usage to string
    sprintf(terminal[i] + visual_len, " %.2f", usage);

    // Prints history of cpu usage and graphics
    for (int j = 0; j <= i ; j++){
        printf("%s\n", terminal[j]);
    }
}

void CPUOutput(char terminal[1024][1024], bool graphics, int i, long int* cpu_previous, long int* idle_previous, cpu_stats info){

    /**
    * Prints information about the current CPU usage of the system
    *
    * @terminal: An array of strings that stores the terminal output
    * @graphics: A boolean value indicating whether graphics option has been selected
    * @i: An integer representing the 
    * @cpu_previous: A pointer to a long int to store the previous CPU usage 
    * @idle_previous: A pointer to a long int to store the previous idle time
    *
    * Function gets CPu usage using sysinfo function from sys/sysinfo.h library
    *
    */

    
    // Calculates total usage of cpu
    long int cpu_total = info.user + info.nice + info.system + info.iowait + info.irq + info.softirq;

    // If -1 is passed as iteration then we only define previous values to prevent seg fault
    if (i == -1){
        *cpu_previous = cpu_total;
        *idle_previous = idle;
        return;
    }

    // Cpu value calculations (Same as assignment)
    long int total_prev = *cpu_previous + *idle_previous;
    long int total_cur = idle + cpu_total;
    double totald = (double) total_cur - (double) total_prev;
    double idled = (double) idle - (double) *idle_previous;
    double cpu_use = fabs((1000 * (totald - idled) / (totald + 1e-6) + 1) / 10);

    if (cpu_use > 100){ cpu_use = 100; }

    // Makes the previous usage equal to the current for the next iteration
    *cpu_previous = cpu_total;
    *idle_previous = idle;

    // Prints the number of cores and cpu usage
    long int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores < 0) {
        fprintf(stderr, "Error: failed to get the number of cores. (%s)\n", strerror(errno));
        return;
    }

    printf("--------------------------------------------\n");
    printf("Number of Cores: %ld\n", num_cores);
    printf(" total cpu use: %.2f%%\n", cpu_use);

    // If graphics have been slected then call the graphics function to add the visuals
    if (graphics){ CPUGraphics(terminal, cpu_use, i); }
    
}

void display(int samples, int tdelay, bool system, bool user, bool graphics, bool sequential){
    /**
    * Outputs all the system information according to the command line arguments selected by user
    * 
    * @samples: the number of times the information will be displayed
    * @tdelay: the time delay between each sample in seconds
    * @system: boolean value indicating whether systems information has been selected
    * @user: boolean value indicating whether user information has been selected
    * @graphics: boolean value indicating whether graphics output has been selected
    * @sequential: boolean value indicating whether equential output has been selected
    * 
    * Displays header, system output, user output, cpu output, and footer
    * Graphics adds visuals to memeory and cpu usage
    * Equential prints information in sequential manner
    */

    // Initialize signal handler
    struct sigaction act;
    act.sa_handler = signal_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("sigaction error for SIGINT");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGTSTP, &act, NULL) == -1) {
        perror("sigaction error for SIGTSTP");
        exit(EXIT_FAILURE);
    }


    // Initialize variables for terminal and cpu output, aswell as previous values for memeory and cpu usage
    char terminal_memory_output[1024][1024];
    char CPU_output[1024][1024];
    double memory_previous;
    long int cpu_previous = 0, idle_previous = 0;

    // Create Pipes
    int pipefd_memory[2], pipefd_cpu[2], pipefd_user[2];

    // Initialize the cpu information with -1 iteration
    CPUOutput(CPU_output, graphics, -1, &cpu_previous, &idle_previous);
    


    // Loop samples number of times
    for (int i = 0; i < samples; i++){

        // Create the pipes
        if (pipe(pipefd_memory) == -1 || pipe(pipefd_cpu) == -1 || pipe(pipefd_user) == -1) {
            fprintf(stderr, "Error: pipe creation failed. (%s)\n", strerror(errno));
            exit(1);
        }

        pid_t pid_memory = fork();

        if (pid_memory == -1) {
            fprintf(stderr, "Error: fork failed. (%s)\n", strerror(errno));
            exit(1);
        }
        else if (pid_memory == 0) {
            // Child process
            close(pipefd_memory[0]); // Close unused read 
            close(pipefd_cpu[0]); close(pipefd_cpu[1]); close(pipefd_user[0]); close(pipefd_user[1]); 
            dup2(pipefd_memory[1], STDOUT_FILENO); // Redirect stdout to the pipe
            close(pipefd_memory[1]); // Close unused write end
            // Memory
            exit(0);
        } 
        else {

            pid_t pid_users = fork();

            if (pid_users == -1) {
                fprintf(stderr, "Error: fork failed. (%s)\n", strerror(errno));
                exit(1);
            }
            else if (pid_users == 0) {
                // Child process
                close(pipefd_user[0]); // Close unused read 
                close(pipefd_memory[0]); close(pipefd_memory[1]); close(pipefd_cpu[0]); close(pipefd_cpu[1]); 
                dup2(pipefd_user[1], STDOUT_FILENO); // Redirect stdout to the pipe
                close(pipefd_user[1]); // Close unused write end
                userOutput(pipefd_user);
                exit(0);
            } 
            else {

                pid_t pid_cpu = fork();

                if (pid_cpu == -1) {
                    fprintf(stderr, "Error: fork failed. (%s)\n", strerror(errno));
                    exit(1);
                }
                else if (pid_cpu == 0) {
                    // Child process
                    close(pipefd_cpu[0]); // Close unused read 
                    close(pipefd_memory[0]); close(pipefd_memory[1]); close(pipefd_user[0]); close(pipefd_user[1]); 
                    dup2(pipefd_cpu[1], STDOUT_FILENO); // Redirect stdout to the pipe
                    close(pipefd_cpu[1]); // Close unused write end
                    // Cpu
                    exit(0);
                }
                else{
                    
                    // Main partent Process
                    // Close the write end of the pipe for the parent
                    close(pipefd_cpu[1]); close(pipefd_memory[1]); close(pipefd_user[1]);
                    // Wait for all child processes to finish
                    waitpid(pid_memory, NULL, 0);
                    waitpid(pid_users, NULL, 0);
                    waitpid(pid_cpu, NULL, 0);


                
                    // If sequential is selected then we do not reset terminal between iterations and state iteration number
                    if (!sequential){ printf("\033[2J \033[1;1H\n"); }
                    else { printf(">>> iteration %d\n", i); }

                    // Displays header information
                    headerUsage(samples, tdelay);

                    // If system is slected diplays systems information usinf systemOutput function
                    if (system){
                        createMemoryChildProcess(pipefd_memory, graphics, i, &memory_previous);
                        close(pipefd_memory[1]);
                        read(pipefd_memory[0], terminal_memory_output[i], sizeof(terminal_memory_output[i]));
                        printf("%s\n", terminal_memory_output[i]);
                        for (int j = 0; j < samples - i - 1; j++){ printf("\n"); }
                    }
                    
                    // If user is slected diplays user information usinf userOutput function
                    if (user){ createUserChildProcess(pipefd_user); }
                    // Displays cpu information using CPUOutput function is system is selected
                    if (system){ CPUOutput(CPU_output, graphics, i, &cpu_previous, &idle_previous); }

                    // Delay the output for tdelay seconds
                    sleep(tdelay);

                    // Close the pipes for reading 
                    close(pipefd_memory[0]);
                    close(pipefd_cpu[0]);
                    close(pipefd_user[0]);

                    // Displays footer
                    footerUsage();

                }
            }
        }
    }
}

int main(int argc, char *argv[]){
    /**
    * Entry point of the program, parses command line arguments to pass values to display function
    *
    * @argc: Number of command-line arguments
    * @argv: Array of pointers to command-line arguments as strings
    *
    * Return: 0 on success, non-zero on error
    */

    // Default values if not specified
    int samples = 10; int tdelay = 1;
    bool system = true; bool user = true; bool graphics = false; bool sequential = false;

    // boolean values to check if arguments have been seen previously
    bool found = false;
    bool user_specified = false, system_specified = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++){
        if (strcmp(argv[i], "--system") == 0 || strcmp(argv[i], "-s") == 0){
            system = true; system_specified = true;
            if (!user_specified){ user = false; }
        }
        else if (strcmp(argv[i], "--user") == 0 || strcmp(argv[i], "-u") == 0){
            user = true; user_specified = true;
            if (!system_specified){ system = false; }
        }
        else if (strcmp(argv[i], "--graphics") == 0 || strcmp(argv[i], "-g") == 0){
            graphics = true;
        }
        else if (strcmp(argv[i], "--sequential") == 0 || strcmp(argv[i], "-seq") == 0){
            sequential = true;
        }
        else if (strncmp(argv[i], "--samples=", 10) == 0){
            // Gets integer in string, and sets found to true to indicate that samples have been seen
            sscanf(argv[i] + 10, "%d", &samples);
            found = true;
        }
        else if (strncmp(argv[i], "--tdelay=", 9) == 0){
            sscanf(argv[i] + 9, "%d", &tdelay);
        }
        // If integer is passed as command line argument the first is samples and the second is delay
        else if (isdigit(*argv[i])){
            if (!found){
                samples = atoi(argv[i]);
                found = true;
            }
            else{ tdelay = atoi(argv[i]); }
        }
    }

    display(samples, tdelay, system, user, graphics, sequential);

    return 0;

}
