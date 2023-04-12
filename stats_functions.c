#include "stats_functions.h"

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
        kill(getpid(), SIGTERM); // Terminate the current process
        kill(getppid(), SIGTERM); // Terminate the parent process
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
        kill(getpid(), SIGTERM); // Terminate the current process
        kill(getppid(), SIGTERM); // Terminate the parent process
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
        kill(getpid(), SIGTERM); // Terminate the current process
        kill(getppid(), SIGTERM); // Terminate the parent process
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
        kill(getpid(), SIGTERM); // Terminate the current process
        kill(getppid(), SIGTERM); // Terminate the parent process
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
    if (sysinfo(&memory) == -1) {
        fprintf(stderr, "Error: failed to get system information. (%s)\n", strerror(errno));
        
        // Terminate the process and its parent if desired
        fprintf(stderr, "Error occurred, terminating the process and its parent.\n");
        kill(getpid(), SIGTERM); // Terminate the current process
        kill(getppid(), SIGTERM); // Terminate the parent process
        return;
    }
    
    
    // Add the memmory usage to terminal
    sprintf(terminal[i], "%.2f GB / %.2f GB -- %.2f GB / %.2f GB", info.used_memory, info.total_memory, info.used_virtual, info.total_virtual);

    // If graphics is enabled then add the visual from the graphics function
    if (graphics){ 
        char graphics_output[1024]; memoryGraphicsOutput(graphics_output, info.used_memory, memory_previous, i);
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

    // Initialize and open utmp
    struct utmp *utmp;
    if (utmpname(_PATH_UTMP) == -1) {
        perror("Error setting utmp file");
        kill(getpid(), SIGTERM); // Terminate the current process
        kill(getppid(), SIGTERM); // Terminate the parent process
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
                kill(getpid(), SIGTERM); // Terminate the current process
                kill(getppid(), SIGTERM); // Terminate the parent process
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
        kill(getpid(), SIGTERM); // Terminate the current process
        kill(getppid(), SIGTERM); // Terminate the parent process
    }
    
    // Opens proc stat file with cpu usage
    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: failed to open /proc/stat. (%s)\n", strerror(errno));
        kill(getpid(), SIGTERM); // Terminate the current process
        kill(getppid(), SIGTERM); // Terminate the parent process
    }

    int read_items = fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld", &info.user, &info.nice, &info.system, &info.idle, &info.iowait, &info.irq, &info.softirq);
    fclose(fp);

    // Checks that all the items have been read
    if (read_items != 7) {
        fprintf(stderr, "Error: failed to read CPU values from /proc/stat. Read %d items instead of 7.\n", read_items);
        kill(getpid(), SIGTERM); // Terminate the current process
        kill(getppid(), SIGTERM); // Terminate the parent process
    }

    ssize_t bytes_written = write(pipefd[1], &info, sizeof(info));

    if (bytes_written == -1) {
        perror("Error writing to pipe");
        kill(getpid(), SIGTERM); // Terminate the current process
        kill(getppid(), SIGTERM); // Terminate the parent process
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

    // Cpu value calculations (Same as assignment)
    long int total_prev = *cpu_previous + *idle_previous;
    long int total_cur = info.idle + cpu_total;
    double totald = (double) total_cur - (double) total_prev;
    double idled = (double) info.idle - (double) *idle_previous;
    double cpu_use = fabs((1000 * (totald - idled) / (totald + 1e-6) + 1) / 10);

    if (cpu_use > 100){ cpu_use = 100; }

    // Makes the previous usage equal to the current for the next iteration
    *cpu_previous = cpu_total;
    *idle_previous = info.idle;

    // Prints the number of cores and cpu usage
    long int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores < 0) {
        fprintf(stderr, "Error: failed to get the number of cores. (%s)\n", strerror(errno));
        kill(getpid(), SIGTERM); // Terminate the current process
        kill(getppid(), SIGTERM); // Terminate the parent process
    }

    printf("--------------------------------------------\n");
    printf("Number of Cores: %ld\n", num_cores);
    printf(" total cpu use: %.2f%%\n", cpu_use);

    // If graphics have been slected then call the graphics function to add the visuals
    if (graphics){ CPUGraphics(terminal, cpu_use, i); }
    
}
