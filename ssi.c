#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>      // For getlogin(), gethostname()
#include <sys/types.h>   // For pid_t
#include <sys/wait.h>    // For wait()
#include <signal.h>      // For signal handling
#include <errno.h> 
#define MAX_HOSTNAME_LEN 256 // Reasonable buffer size for host name
#define MAX_CWD_LEN 1024     // Reasonable length for current working directory
#define MAX_INPUT_LEN 1024   // Maximum length for user input

pid_t child_pid = -1; // Global variable to store the current child process ID

// Signal handler for SIGINT (Ctrl-C)
void handle_sigint(int sig)
{
    if (child_pid > 0) // If a child process is running
    {
        kill(child_pid, SIGINT); // Send SIGINT to the child process
    }
    else
    {
        printf("\n"); // If no child process, just print a newline
    }
}

void handle_pwd()
{
    char cwd[MAX_CWD_LEN]; // Buffer to store the current working directory

    // Get the current working directory and print it
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s\n", cwd);
    }
    else
    {
        perror("getcwd failed"); // Handle errors in getting the current directory
    }
}
// Function to handle the cd command

// Function to handle the cd command
void handle_cd(char *path) {
    // Case 1: No path specified, go to the home directory
    if (path == NULL || strcmp(path, "") == 0) {
        path = getenv("HOME");
        if (path == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
    }

    // Case 2: Path starts with ~, replace with home directory
    else if (path[0] == '~') {
        char *home = getenv("HOME");
        if (home == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }

        // Construct the full path by replacing ~ with the home directory path
        char full_path[MAX_CWD_LEN];
        snprintf(full_path, sizeof(full_path), "%s%s", home, path + 1); // Skip the ~ character
        path = full_path; // Update path to the full path
    }

    // Change the directory using chdir()
    if (chdir(path) != 0) {
        perror("cd failed"); // Handle errors in changing the directory
    }
}


int main()
{
    char hostname[MAX_HOSTNAME_LEN]; // Buffer to store the hostname
    char cwd[MAX_CWD_LEN];           // Buffer to store current working directory
    const char *username;            // Pointer to store username

    int bailout = 0;
    username = getlogin();

    if (!username)
    {
        perror("get login error");
        return 1;
    }

    if (gethostname(hostname, sizeof(hostname)) != 0)
    {
        perror("gethostname failed");
        return 1;
    }

    // Set up the signal handler for Ctrl-C (SIGINT)
    signal(SIGINT, handle_sigint);

    while (!bailout)
    {
        // Get current working directory
        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            perror("getcwd failed");
            return 1;
        }

        char prompt[MAX_CWD_LEN + MAX_HOSTNAME_LEN + 50]; // Extra 50 for additional characters
        snprintf(prompt, sizeof(prompt), "%s@%s: %s > ", username, hostname, cwd); // Limits the copy to the buffer size

        char *reply = readline(prompt);

        if (!reply) // Check if reply is NULL (e.g., if the user types Ctrl+D)
        {
            printf("\n");
            break;
        }

        // Check if the input is "bye"
        if (!strcmp(reply, "bye"))
        {
            bailout = 1;
        }

	 else if (!strcmp(reply, "pwd"))
        {
            handle_pwd(); // Call the function to print the current working directory
        }
	else if (strncmp(reply, "cd", 2) == 0) {
            // Extract the path after "cd", accounting for space or empty input
            char *path = reply + 2; // Skip "cd"
            while (*path == ' ') path++; // Skip leading spaces to get the path
            handle_cd(path); // Call the function to change the directory
        }

        else 
        {
            // Fork a new process to execute the command
            child_pid = fork();
            if (child_pid < 0)
            {
                perror("fork failed");
            }
            else if (child_pid == 0) // Child process
            {
                // Tokenize the input for command and parameters
                char *args[MAX_INPUT_LEN / 2 + 1]; // Allocate space for command and arguments
                char *token = strtok(reply, " ");  // Split by space
                int i = 0;

                // Fill the args array
                while (token != NULL && i < (MAX_INPUT_LEN / 2))
                {
                    args[i++] = token;          // Add token to args
                    token = strtok(NULL, " ");  // Get next token
                }
                args[i] = NULL; // Null-terminate the arguments array

                // Execute the command
                execvp(args[0], args); // Execute the command with parameters
                  // If execvp fails, print a custom error message
                if (errno == ENOENT)
                {
                    fprintf(stderr, "%s: No such file or directory\n", args[0]);
                }
                else
                {
                    perror("execvp failed"); // For other errors, use perror
                }
                exit(1);                 // Exit the child process
            }
            else // Parent process
            {
                // Wait for the child process to complete or be interrupted
                int status;
                waitpid(child_pid, &status, 0);
                child_pid = -1; // Reset the child process ID after it finishes
            }
        }

        free(reply);
    }
    printf("Bye Bye\n");
    return 0;
}

