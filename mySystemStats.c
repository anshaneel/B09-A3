#include "stats_functions.h"

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
    // CPUOutput(CPU_output, graphics, -1, &cpu_previous, &idle_previous);



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
            
            // Memory
            memoryStats(pipefd_memory);
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
                // User
                userOutput(pipefd_user);
                close(pipefd_user[1]); // Close unused write end
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
                    // Cpu
                    cpuStats(pipefd_cpu);
                    close(pipefd_cpu[1]); // Close unused write end
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
                        // Read system data from the pipe
                        memory received_info;
                        ssize_t bytes_read = read(pipefd_memory[0], &received_info, sizeof(received_info));

                        if (bytes_read == -1) { perror("Error reading from pipe"); }

                        systemOutput(terminal_memory_output, graphics, i, &memory_previous, received_info);
                        for (int j = 0; j < samples - i - 1; j++){ printf("\n"); }

                        // Close Pipe after reading
                        close(pipefd_memory[0]);
                    }
                    
                    // If user is slected diplays user information usinf userOutput function
                    if (user){ 
                        // Read User data from pipe
                        // Read user information from the pipe
                        char buffer[1024];
                        int bytesRead;

                        // Print Divider
                        printf("--------------------------------------------\n");
                        printf("### Sessions/users ###\n");

                        while ((bytesRead = read(pipefd_user[0], buffer, sizeof(buffer) - 1)) > 0) {
                            buffer[bytesRead] = '\0';
                            printf("%s", buffer);
                        }
                        close(pipefd_user[0]); // Close read end after reading
                     }

                    // Displays cpu information using CPUOutput function is system is selected
                    if (system){ 
                        // Read system data from the pipe
                        cpu_stats received_info;
                        ssize_t bytes_read = read(pipefd_cpu[0], &received_info, sizeof(received_info));

                        if (bytes_read == -1) { perror("Error reading from pipe"); }

                        CPUOutput(CPU_output, graphics, i, &cpu_previous, &idle_previous, received_info); 

                        // Close Pipe after reading
                        close(pipefd_cpu[0]);
                        }

                    // Delay the output for tdelay seconds
                    sleep(tdelay);

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
