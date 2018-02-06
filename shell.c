//
//  shell.c
//  Assignment_1
//
//  Created by Alexander Wu on 2017-01-23.
//  Copyright © 2017 AlexanderWu. All rights reserved.
//

// C libraries required
#include <stdio.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>

// Shell prompt string
const char* prompt = "\n>> ";
// Maximum Number of Jobs
const int MAX_JOBS = 10;
// Maximum Path Size for the pwd command
const int MAX_PATH_SIZE = 512;
// Global pid for signal call back function, needed to only kill fg process
pid_t global_pid;
// Maximum File Size for Piping
const int MAX_SIZE = 1024;

//
// Signal Handler Callback function
//
static void sigHandler(int sig) {
    // SIGINT corresponds to Ctrl-C and only kills fg process
    // Ctrl-C does nothing if there is no program running on the shell
    if (sig == SIGINT && global_pid != 0) {
        /////////////////////////////////////////////////////////////////////////////////////////////////
        /////////////////////////////////////////////////////////////////////////////////////////////////
        //                                                                                             //
        //                 NOTE: Ctrl-C only kills programs and not the shell. If no program           //
        //                 is running, then Ctrl-C will only be ignored. Instructions weren't          //
        //                 100% clear so I just decided to imitate the linux shell which does          //
        //                 the same (kills runnign program and just ignores signal if nothing)         //
        //                 is running!!!                                                               //
        //                                                                                             //
        /////////////////////////////////////////////////////////////////////////////////////////////////
        /////////////////////////////////////////////////////////////////////////////////////////////////
        kill(global_pid, SIGKILL);
    }
}

//
// Modify Signals function that takes cares of Ctrl-Z and Ctrl-C signals
//
void modify_signals() {
    // SIGTSTP corresponds to Ctrl-Z and SIG_IGN ignores the signal
    if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
        printf("ERROR! Could not bind the signal handler\n");
        exit(EXIT_FAILURE);
    }
}

//
// Parse function that parses stdin args
//
int parser(char* line, char* args[], int* bg) {
    // Use strsep to extract tokens (args)
    char* token;
    int i = 0;
    *bg = 0;

    // New ptr to the line for memory leaks
    char* line2 = line;

    while ((token = strsep(&line2, " \t\n")) != NULL) {
        for (int j = 0; j < strlen(token); j++) {
            if (token[j] <= 32) {
                token[j] = '\0';
            }
        }
        if (strlen(token) > 0) {
            // Duplicate memory and string so that we can free it after
            char* tmp = strdup(token);
            args[i++] = tmp;
        }
    }
    // Check for '&' symbol only if its at the end
    // Thus, it will not work if you do "sleep & 10"
    if (i > 0 && strcmp(args[i-1], "&") == 0) {
        *bg = 1;
        // remove argument
        args[--i] = NULL;
    }
    return i;
}

//
// Get command arguments from stdin
//
int getcmd(char* args[], int* background) {

    int linelength = 0;
    char* line = NULL;
    size_t linecap = 0;
    
    printf("%s", prompt);
    linelength = getline(&line, &linecap, stdin);
    
    // Error
    if (linelength <= 0) {
        exit(-1);
    }
    else {
        int nbArgs = parser(line, args, background);
        // Free line to avoid memory leaks
        free(line);
        return nbArgs;
    }
}

//
// Built-In Change Directory Command
//
void execute_cd(char* directory) {
    int success = chdir(directory);
    if (success == -1) {
        printf("Wrong directory specified");
    }
}

//
// Built-In Print Working Directory Command
//
void execute_pwd() {
    // Create a buffer for the path
    char path[MAX_PATH_SIZE];
    getcwd(path,MAX_PATH_SIZE);
    printf("%s\n", path);
}

//
// Built-In Exit Command
//
void execute_exit() {
    exit(EXIT_SUCCESS);
}

//
// Built-In Foreground Command that puts job to foreground
//
void execute_fg(char* job_nb, pid_t jobs[]) {
    // Parse string into int
    int job = atoi(job_nb);
    // Check if it is a valid job number and if it is running
    if (job > -1 && job <= MAX_JOBS && jobs[job] != 0 && kill(jobs[job],0) == 0) {
        // Run job in foreground
        kill(jobs[job], SIGCONT);
        // Have the parent process wait for child to finish
        waitpid(jobs[job], NULL, WUNTRACED);
    }
    else {
        printf("Error: Invalid Job number\n");
    }
}

//
// Built-In Job Command that prints all the background jobs that are running
//
void execute_jobs(pid_t jobs[]) {
    printf("--------------------------------------------------------\n");
    printf("Background Jobs:\tStatus\t\t\tPID\n");
    // Iterate through all possible job numbers
    int i;
    for (i=0; i < MAX_JOBS; i++) {
        if (jobs[i] != 0) {
            // WNOHANG is a non blocking call
            // Use of waitpid for child process to exit if it's done
            waitpid(jobs[i], NULL, WNOHANG);
            // Check if job is still running
            if (kill(jobs[i],0) == 0) {
                printf("[%d]\t\t\tRunning\t\t\t%d\n", i, jobs[i]);
            }
        }
        else {
            // Process is not running
            jobs[i] = 0;
        }
    }
    printf("--------------------------------------------------------\n");
}

//
// Save each background job's PID
//
void save_jobs(pid_t pid, pid_t jobs[], int* job_index) {
    jobs[*job_index] = pid;
    *job_index = (*job_index + 1) % MAX_JOBS;
}


int main() {

    // Background variable for the Aperand symbol
    int bg ;
    // Array of each job's PID
    pid_t jobs[MAX_JOBS];
    // Index of last job added
    int latest_job_index = 0;

    // Standard output file descriptor backup
    int stdout_backup_fd = -1;
    // Output redirection fd
    int output_red_fd = -1;


    // Modify signals such as Ctrl-Z and Ctrl-C
    modify_signals();
    
    // Infinite loop
    while (1) {

        // Initialize background and arguments variables
        char* args[20] = { NULL };
        bg = 0;

        // boolean in the form of an integer to indicate if program requires piping
        int isPipe = 0;        
        // Index of '|' or '>' commands
        int special_index = -1;

        // Get user's arguments
        int nbArgs = getcmd(args, &bg);
        // Reinitialize special_index;
        special_index = -1;
        
        // Check for output redirection and command piping
        if (nbArgs > 2) {
            // Look for '|' and '>' arguments
            int i;
            for (i=0; i < nbArgs; i++) {
                if (strcmp(args[i],">") == 0 || strcmp(args[i],"|") == 0) {
                    special_index = i;
                    break;
                }
            }


            if (special_index != -1) {

                // Output Redirection
                if (strcmp(args[special_index], ">") == 0) {

                    // save stdout's file descriptor as backup
                    stdout_backup_fd = dup(STDOUT_FILENO);
                    close(STDOUT_FILENO);

                    // Create a new file descriptor 
                    // And switch stdout's fd as input to output file
                    output_red_fd = open(args[nbArgs-1], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IXUSR);

                    // Remove arguments
                    args[special_index] = NULL;
                    args[nbArgs-1] = NULL;
                    nbArgs -= 2;               
                }
                // Command Piping
                else if (strcmp(args[special_index], "|") == 0) {

                    isPipe = 1;
                    int fd_pipe[2];

                    if (pipe(fd_pipe) == -1 ){
                        printf("Error: Error in piping\n");
                        exit(EXIT_FAILURE);
                    }

                    pid_t first_child = fork();
                    // First fork for left side arguments which sends to STDOUT
                    if (first_child == (pid_t)0) {
                        // Close stdout and have fd_pipe point to fd of stdout
                        close(STDOUT_FILENO);
                        dup(fd_pipe[1]);
                        close(fd_pipe[0]);
                        close(fd_pipe[1]);                        

                        // Remove arguments that are not part of the left side arguments
                        int i;
                        for (i=special_index; i<nbArgs; i++) {
                            args[i] = NULL;
                        }
                        nbArgs -= (nbArgs-special_index);

                        execvp(args[0], args);
                    }
                    // Error during Fork
                    else if (first_child == (pid_t)-1) {
                        printf("Error: Error occured during forking\n");
                        exit(EXIT_FAILURE);
                    }

                    pid_t second_child = fork();

                    // Error during Fork
                    if (second_child == (pid_t)-1) {
                        printf("Error: Error occured during forking\n");
                        exit(EXIT_FAILURE);
                    }
                    // Second fork for right side arguments which reads from STDIN
                    else if (second_child == (pid_t) 0) {
                        // Close stdin and have fd_pipe point to the stdin
                        close(STDIN_FILENO);
                        dup(fd_pipe[0]);
                        close(fd_pipe[1]);
                        close(fd_pipe[0]);  

                        // Remove arguments that are not part of the right side
                        int i;
                        for (i=special_index; i<nbArgs-1; i++) {
                            args[i-special_index] = args[i+1];
                            args[i+1] = NULL;
                        }
                        while (args[i-special_index] != NULL) {
                            args[i-special_index] = NULL;
                            i++;
                        }
                        nbArgs -= (special_index+1);

                        // Exec Arguments
                        execvp(args[0], args);
                    }
                    // close both pipes and have the parent wait for both child 
                    // to complete their tasks
                    close(fd_pipe[0]);
                    close(fd_pipe[1]);
                    waitpid(first_child, NULL, WUNTRACED);
                    waitpid(second_child, NULL, WUNTRACED);
                }
            }
        }


        // Check for empty arguments or skip if pipe is used
        if (args[0] == NULL || isPipe == 1) {
        }
        // Built-in Commands
        else if (nbArgs == 2 && strcmp(args[0],"cd") == 0) {
            execute_cd(args[1]);
        }
        else if (nbArgs == 1 && strcmp(args[0],"pwd") == 0) {
            execute_pwd();
        }
        else if (nbArgs == 1 && strcmp(args[0],"exit") == 0) {
            execute_exit();
        }
        else if (nbArgs == 2 && strcmp(args[0],"fg") == 0 ) {
            execute_fg(args[1], jobs);
        }
        else if (nbArgs == 1 && strcmp(args[0],"jobs") == 0) {
            execute_jobs(jobs);
        } 
        // External Commands
        else {
            pid_t pid = fork();
            // Error
            if (pid == (pid_t)-1) {
                printf("Error: Error occured during forking\n");
                exit(EXIT_FAILURE);
            }
            // Child Process
            else if (pid == (pid_t) 0) {
                // External Commands
                execvp(args[0], args);
                printf("Error: Error occured during execvp\n");
                exit(EXIT_FAILURE);
            }
            // Parent Process
            else {
                if (bg == 0) {
                    // global_pid for signals which will allow us to kill fg process only
                    global_pid = pid;

                    // Ctrl-C signal
                    if (signal(SIGINT, sigHandler) == SIG_ERR) {
                        printf("ERROR! Could not bind the signal handler\n");
                        exit(EXIT_FAILURE);
                    }                    

                    waitpid(pid, NULL, WUNTRACED);
                    // reset global pid when child process running program is done
                    global_pid = 0;
                }
                // Save background job
                else {
                    // Ignore Ctrl-C for background processes
                    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
                        printf("ERROR! Could not bind the signal handler\n");
                        exit(EXIT_FAILURE);
                    } 
                    save_jobs(pid, jobs, &latest_job_index);
                }
            }
        }

        // Restore stdout to original file descriptor after using output redirection
        if (special_index != -1 && isPipe == 0) {
            // Close output redirection fd
            close(output_red_fd);
            // repoint stdout to old fd
            dup2(stdout_backup_fd, STDOUT_FILENO);
            close(stdout_backup_fd);
            output_red_fd = -1;
        } 
        int i=0;
        // Free args to avoid memory leaks
        while (args[i] != NULL) {
            free(args[i]);
            i++;
        }
    }
    return EXIT_SUCCESS;
}