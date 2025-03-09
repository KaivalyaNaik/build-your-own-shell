#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// Structures
typedef struct Command {
    char *command;
    char *operator;  // Operator that follows this command (&&, ||, or NULL)
} Command;

// Function prototypes
void cash();
char** splitinput(char *buff);
int executecmd(char** tokens);
int checkAndExecuteInbuiltFunctions(char** tokens);
Command* parseInput(char *buff, int *ncom);
int executeCommandSequence(char* command);
int executeCommands(Command* commands, int n);
void rot13(char *str);
char* trim(char* str);

/**
 * Trim leading and trailing whitespace
 * Returns a pointer within the original string - do not free this pointer separately
 */
char* trim(char* str) {
    if (!str) return NULL;
    
    // Trim leading spaces
    while (*str == ' ') str++;  
    
    // If the string is now empty, return it
    if (*str == '\0') return str;
    
    // Trim trailing spaces
    char* end = str + strlen(str) - 1;
    while (end > str && *end == ' ') end--;
    *(end + 1) = '\0';
    
    return str;
}

/**
 * Main shell function
 */
void cash() {
    char buff[1024] = {0};
    int buff_pos = 0;
    int continuation = 0;  // Flag to track if we're continuing from previous line
    
    while(1) {
        if (!continuation) {
            printf("$ ");
        }
        fflush(stdout);
        
        char line[1024];
        if (fgets(line, sizeof(line), stdin) == NULL)
            exit(EXIT_SUCCESS);
            
        int line_len = strlen(line);
        
        // Remove trailing newline
        if (line_len > 0 && line[line_len-1] == '\n') {
            line[--line_len] = '\0';
        }
        
        // Check if line ends with backslash (line continuation)
        if (line_len > 0 && line[line_len-1] == '\\') {
            // Remove the backslash
            line[--line_len] = '\0';
            continuation = 1;
        } else {
            continuation = 0;
        }
        
        // Append this line to our command buffer
        strcpy(buff + buff_pos, line);
        buff_pos += line_len;
        
        // If we're not continuing, execute the command
        if (!continuation) {
            // Skip empty commands
            if (strlen(trim(buff)) > 0) {
                executeCommandSequence(buff);
            }
            
            // Reset the buffer for the next command
            memset(buff, 0, sizeof(buff));
            buff_pos = 0;
        }
    }
}

/**
 * Execute a sequence of commands, handling subshells and logical operators
 */
int executeCommandSequence(char* command) {
    command = trim(strdup(command));
    
    // Check if this is a subshell command (enclosed in parentheses)
    int len = strlen(command);
    if (len >= 2 && command[0] == '(' && command[len-1] == ')') {
        // Extract the command inside parentheses
        char* subshellCmd = malloc(len - 1);
        strncpy(subshellCmd, command + 1, len - 2);
        subshellCmd[len - 2] = '\0';
        
        // Create a child process for the subshell
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            free(subshellCmd);
            free(command);
            return 0;
        }
        
        if (pid == 0) {
            // Child process - execute the subshell commands and exit
            int result = executeCommandSequence(subshellCmd);
            free(subshellCmd);
            free(command);
            exit(result ? EXIT_SUCCESS : EXIT_FAILURE);
        } else {
            // Parent process - wait for the subshell to complete
            int status;
            waitpid(pid, &status, 0);
            int subshellResult = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            
            // If this is a simple subshell command (not part of a larger command),
            // return the result and free resources
            if (command[0] == '(' && command[len-1] == ')' && 
                !strstr(command, "&&") && !strstr(command, "||")) {
                free(subshellCmd);
                free(command);
                return subshellResult;
            }
            
            // For test cases like "(exit 0 && exit 1) && echo-rot13 foo"
            // We need to process the larger command using the result from the subshell
            char* remainingCmd = NULL;
            
            // Check if there's a larger command after the subshell
            char* afterParen = strstr(command, ")");
            if (afterParen && (strstr(afterParen, "&&") || strstr(afterParen, "||"))) {
                // Get the operator
                char* opStart = NULL;
                if (strstr(afterParen, "&&")) {
                    opStart = strstr(afterParen, "&&");
                } else {
                    opStart = strstr(afterParen, "||");
                }
                
                // If subshell failed, and operator is &&, skip next command
                if (!subshellResult && strncmp(opStart, "&&", 2) == 0) {
                    free(subshellCmd);
                    free(command);
                    return 0;
                }
                
                // If subshell succeeded, and operator is ||, skip next command
                if (subshellResult && strncmp(opStart, "||", 2) == 0) {
                    free(subshellCmd);
                    free(command);
                    return 1;
                }
                
                // Execute the right side of the operator
                remainingCmd = trim(opStart + 2);
                if (strlen(remainingCmd) > 0) {
                    char* cmdCopy = strdup(remainingCmd);
                    int result = executeCommandSequence(cmdCopy);
                    free(cmdCopy);
                    free(subshellCmd);
                    free(command);
                    return result;
                }
            }
            
            free(subshellCmd);
            free(command);
            return subshellResult;
        }
    }
    
    // Regular command processing
    int n;
    Command* commands = parseInput(command, &n);
    int result = executeCommands(commands, n);
    
    // Free all commands and operators
    for (int i = 0; i < n; i++) {
        free(commands[i].command);
        if (commands[i].operator != NULL) {
            free(commands[i].operator);
        }
    }
    free(commands);
    free(command);
    
    return result;
}

/**
 * Parse the input string into a sequence of commands with operators
 */
Command* parseInput(char *buff, int *ncom) {
    // Preprocess buff to handle subshells as atomic commands
    char* processedBuff = strdup(buff);
    int i = 0, inSubshell = 0, parenDepth = 0;
    int len = strlen(processedBuff);
    
    // Replace && and || inside subshells with temporary markers
    while (i < len) {
        if (processedBuff[i] == '(') {
            parenDepth++;
            inSubshell = 1;
        } else if (processedBuff[i] == ')') {
            parenDepth--;
            if (parenDepth == 0) {
                inSubshell = 0;
            }
        } else if (inSubshell && processedBuff[i] == '&' && processedBuff[i+1] == '&') {
            // Replace && with a temporary marker inside subshells
            processedBuff[i] = '#';
            processedBuff[i+1] = '#';
            i++;
        } else if (inSubshell && processedBuff[i] == '|' && processedBuff[i+1] == '|') {
            // Replace || with a temporary marker inside subshells
            processedBuff[i] = '@';
            processedBuff[i+1] = '@';
            i++;
        }
        i++;
    }
    
    i = 0;
    int l = strlen(processedBuff);
    int n = 0;
    
    // Count number of commands
    while (i < l) {
        if (processedBuff[i] == '&' && processedBuff[i + 1] == '&') {
            n++;
            i += 2;
        } else if (processedBuff[i] == '|' && processedBuff[i + 1] == '|') {
            n++;
            i += 2;
        } else {
            i++;
        }
    }
    n++;
    Command *commands = (Command *)malloc(n * sizeof(Command));

    // Parse commands
    i = 0;
    int j = 0, k = 0;
    char *tmp = (char *)malloc((l + 1) * sizeof(char));

    while (i < l) {
        if (processedBuff[i] == '&' && processedBuff[i + 1] == '&') {
            tmp[j] = '\0';
            commands[k].command = strdup(tmp);
            commands[k].operator = strdup("&&");
            j = 0;
            k++;
            i += 2;
        } else if (processedBuff[i] == '|' && processedBuff[i + 1] == '|') {
            tmp[j] = '\0';
            commands[k].command = strdup(tmp);
            commands[k].operator = strdup("||");
            j = 0;
            k++;
            i += 2;
        } else {
            tmp[j++] = processedBuff[i++];
        }
    }
    tmp[j] = '\0';
    commands[k].command = strdup(tmp);
    commands[k].operator = NULL;
    *ncom = n;
    free(tmp);
    free(processedBuff);
    
    // Restore the temporary markers back to && and || in the command strings
    for (i = 0; i < n; i++) {
        char* cmd = commands[i].command;
        int cmdLen = strlen(cmd);
        for (j = 0; j < cmdLen - 1; j++) {
            if (cmd[j] == '#' && cmd[j+1] == '#') {
                cmd[j] = '&';
                cmd[j+1] = '&';
            } else if (cmd[j] == '@' && cmd[j+1] == '@') {
                cmd[j] = '|';
                cmd[j+1] = '|';
            }
        }
    }
    
    return commands;
}

/**
 * Execute a sequence of commands with logical operators
 */
int executeCommands(Command* commands, int n) {
    int prevCmd = 1;  // Initialize to success for first command
    
    for (int i = 0; i < n; i++) {
        // Determine whether to execute this command based on logical operators
        int shouldExecute = 0;
        
        if (i == 0) {
            // First command always executes
            shouldExecute = 1;
        } else if (strcmp(commands[i-1].operator, "&&") == 0) {
            // Execute if previous command succeeded
            shouldExecute = (prevCmd == 1);
        } else if (strcmp(commands[i-1].operator, "||") == 0) {
            // Execute if previous command failed
            shouldExecute = (prevCmd == 0);
        }
        
        if (shouldExecute) {
            // Process semicolons in the command (multiple commands)
            char* cmd_copy = strdup(commands[i].command);
            char* token;
            char* saveptr;
            int lastResult = 1;
            
            // Process each command separated by semicolons
            token = strtok_r(cmd_copy, ";", &saveptr);
            while (token != NULL) {
                char* clean_token = trim(token);
                
                // Skip empty commands
                if (strlen(clean_token) > 0) {
                    // Check if this is a subshell command (enclosed in parentheses)
                    int len = strlen(clean_token);
                    if (len >= 2 && clean_token[0] == '(' && clean_token[len-1] == ')') {
                        // Extract and execute the subshell command
                        char* subshellCmd = malloc(len - 1);
                        strncpy(subshellCmd, clean_token + 1, len - 2);
                        subshellCmd[len - 2] = '\0';
                        
                        // Execute in a child process
                        pid_t pid = fork();
                        if (pid == -1) {
                            perror("fork failed");
                            free(subshellCmd);
                            lastResult = 0;
                        } else if (pid == 0) {
                            // Child process - execute subshell commands
                            int result = executeCommandSequence(subshellCmd);
                            free(subshellCmd);
                            exit(result ? EXIT_SUCCESS : EXIT_FAILURE);
                        } else {
                            // Parent process - wait for completion
                            int status;
                            waitpid(pid, &status, 0);
                            lastResult = WIFEXITED(status) && WEXITSTATUS(status) == 0;
                            free(subshellCmd);
                        }
                    } else {
                        // Regular command
                        char** tokens = splitinput(clean_token);
                        lastResult = executecmd(tokens);
                        free(tokens);
                    }
                }
                
                token = strtok_r(NULL, ";", &saveptr);
            }
            
            free(cmd_copy);
            prevCmd = lastResult;
        }
    }
    
    return prevCmd;
}

/**
 * Split input string into an array of tokens
 */
char** splitinput(char *buff) {
    int i = 0;
    char *token;
    char **tokens = (char**) malloc(sizeof(char*) * 100);  // Assuming max 100 tokens
    
    token = strtok(buff, " ");
    while (token != NULL) {
        tokens[i++] = token;
        token = strtok(NULL, " ");
    }
    tokens[i] = NULL;
    
    return tokens;
}

/**
 * Execute a single command
 */
int executecmd(char** tokens) {
    if (tokens[0] == NULL) {  // Prevent executing an empty command
        return 1;
    }

    // Check for bang (!) operator that negates the exit code
    int negate_result = 0;
    if (strcmp(tokens[0], "!") == 0) {
        negate_result = 1;
        
        // Shift all tokens to remove the "!" operator
        int i = 0;
        while (tokens[i] != NULL) {
            tokens[i] = tokens[i + 1];
            i++;
        }
        
        // Check if there are any tokens left after removing "!"
        if (tokens[0] == NULL) {
            return 0;  // "!" alone means "negate nothing", which is failure
        }
    }

    // Check for built-in functions
    if (checkAndExecuteInbuiltFunctions(tokens)) {
        return negate_result ? 0 : 1;  // Negate if needed
    }

    // Execute external command
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return negate_result ? 1 : 0;  // Negate if needed
    }

    if (pid == 0) {
        // Child process
        if (execvp(tokens[0], tokens) == -1) {
            perror("Command Failed");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        int result = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        return negate_result ? !result : result;  // Negate if needed
    }
}

/**
 * Check and execute built-in functions
 * Returns 1 if a built-in was executed, 0 otherwise
 */
int checkAndExecuteInbuiltFunctions(char** tokens) {
    if (strcmp(tokens[0], "exit") == 0) {
        // Handle exit with optional status code
        if (tokens[1] != NULL) {
            exit(atoi(tokens[1]));
        } else {
            exit(EXIT_SUCCESS);
        }
        return 1;
    } else if (strcmp(tokens[0], "cd") == 0) {
        // Handle cd command
        if (tokens[1] == NULL) {
            fprintf(stderr, "Expected argument to \"cd\"\n");
        } else {
            if (chdir(tokens[1]) != 0) {
                perror("cd failed");
            }
        }
        return 1;
    } else if (strcmp(tokens[0], "echo") == 0) {
        // Handle echo command
        int i = 1;
        int append_newline = 1;
        
        // Check for -n option
        if (tokens[i] != NULL && strcmp(tokens[i], "-n") == 0) {
            append_newline = 0;
            i++;
        }
        
        // Output all arguments
        while (tokens[i] != NULL) {
            printf("%s", tokens[i]);
            if (tokens[i+1] != NULL) {
                printf(" ");
            }
            i++;
        }
        
        // Add newline if not suppressed
        if (append_newline) {
            printf("\n");
        }
        
        return 1;
    } else if (strcmp(tokens[0], "echo-rot13") == 0) {
        // Handle echo-rot13 command
        if (tokens[1] == NULL) {
            printf("\n");
        } else {
            char* str = tokens[1];
            rot13(str);
            printf("%s\n", str);
        }
        return 1;
    } else if (strcmp(tokens[0], "exec") == 0) {
        // Handle exec command
        if (tokens[1] == NULL) {
            fprintf(stderr, "exec: expected command\n");
            return 1;
        }
        
        // Shift all tokens to remove "exec"
        char** newTokens = tokens + 1;
        if (execvp(newTokens[0], newTokens) == -1) {
            perror("exec failed");
            exit(EXIT_FAILURE);
        }
        // Will never reach here if exec succeeds
        return 1;
    }
    
    return 0;
}

/**
 * ROT13 encryption/decryption for testing
 */
void rot13(char *str) {
    if (!str) return;
    
    for (int i = 0; str[i]; i++) {
        if ((str[i] >= 'a' && str[i] <= 'z')) {
            str[i] = 'a' + (str[i] - 'a' + 13) % 26;
        } else if ((str[i] >= 'A' && str[i] <= 'Z')) {
            str[i] = 'A' + (str[i] - 'A' + 13) % 26;
        }
    }
}

/**
 * Main function
 */
int main(int argc, char* argv[]) {
    cash();
    return 0;
}