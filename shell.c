#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>

// Structures
typedef struct Command
{
    char *command;
    char *operator;
} Command;

// Function prototypes
void cash();
char** splitinput(char *buff);
int executecmd(char** tokens);
int checkAndExecuteInbuiltFunctions(char** tokens);
Command* parseInput(char *buff, int *ncom);



char* trim(char* str) {
    while (*str == ' ') str++;  
    char* end = str + strlen(str) - 1;
    while (end > str && *end == ' ') end--;  
    *(end + 1) = '\0';  
    return str;
}


void cash(){
    while(1){
        printf("$ ");
        fflush(stdout);
        char buff[1024];
        if(fgets(buff,1024,stdin)==NULL)
            exit(EXIT_SUCCESS);
        
        int n;
        Command * commands = parseInput(buff,&n);
        int i = 0;
        int prevCmd = 1;  // Initialize to success for first command
        
        while(i<n){
            if(i == 0 || (i > 0 && commands[i-1].operator == NULL)) {
                char** tokens = splitinput(commands[i].command);
                prevCmd = executecmd(tokens);
                free(tokens); 
            }
            else if(commands[i-1].operator != NULL) {
                if(strcmp(commands[i-1].operator,"&&") == 0) {
                    if(prevCmd == 1) {
                        char** tokens = splitinput(commands[i].command);
                        prevCmd = executecmd(tokens);
                        free(tokens);
                    }
                }
                else if(strcmp(commands[i-1].operator,"||") == 0) {
                    if(prevCmd == 0) {
                        char** tokens = splitinput(commands[i].command);
                        prevCmd = executecmd(tokens);
                        free(tokens);
                    }
                }
            }
            i++;
        }
        
        // Free all commands and operators
        for(i = 0; i < n; i++){
            free(commands[i].command);
            if(commands[i].operator != NULL) {
                free(commands[i].operator);
            }
        }
        free(commands);
    }
}

int main(int argc,char **argv){
    cash();
    return EXIT_SUCCESS;
}


Command* parseInput(char *buff, int *ncom) {
    int pos = 0;
    int commandCount = 0;
    Command *commands = malloc(1024 * sizeof(Command));
    char *token;
    char *start = buff;
    
    while (buff[pos] != '\0') {
        // Find the end of the command
        start = &buff[pos];
        while (buff[pos] != '\0' && buff[pos] != ';' && 
               !(buff[pos] == '&' && buff[pos + 1] == '&') && 
               !(buff[pos] == '|' && buff[pos + 1] == '|')) {
            pos++;
        }
        
        // Extract and store the command
        int cmdLen = &buff[pos] - start;
        token = malloc(cmdLen + 1);
        strncpy(token, start, cmdLen);
        token[cmdLen] = '\0';
        commands[commandCount].command = trim(token);
        
        // Check the operator type
        if (buff[pos] == '\0') {
            commands[commandCount].operator = NULL;
        } else if (buff[pos] == ';') {
            commands[commandCount].operator = NULL;
            pos++;
        } else if (buff[pos] == '&' && buff[pos + 1] == '&') {
            commands[commandCount].operator = strdup("&&");
            pos += 2;
        } else if (buff[pos] == '|' && buff[pos + 1] == '|') {
            commands[commandCount].operator = strdup("||");
            pos += 2;
        }
        
        // Skip whitespace after operator
        while (buff[pos] == ' ') pos++;
        
        commandCount++;
    }
    
    *ncom = commandCount;
    return commands;
}


char** splitinput(char *buff){
    int position=0;
    char **tokens = malloc(1024 * sizeof(char*));
    char *tok;

    // Remove newline at the end of input
    buff[strcspn(buff, "\n")] = 0;

    tok = strtok(buff," ");
    while(tok!=NULL){
        tokens[position] = tok;
        position++;
        tok = strtok(NULL," ");
    }
    tokens[position]=NULL;
    return tokens;
}


int executecmd(char** tokens){
    if (tokens[0] == NULL) {  // Prevent executing an empty command
        return 1;
    }

    if(checkAndExecuteInbuiltFunctions(tokens)) //Handle builtin functions
        return 1;

    
    pid_t pid = fork();
    if(pid == -1){
        perror("fork failed");
        return 0;
    }

    if(pid == 0){
        if(execvp(tokens[0],tokens) == -1){
            perror("Command Failed");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }
    else{
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
}

int checkAndExecuteInbuiltFunctions(char** tokens){
    
    if(strcmp(tokens[0],"exit")==0){
        exit(EXIT_SUCCESS);
    }
    else if(strcmp(tokens[0],"cd")==0){
        if(tokens[1] == NULL){
            perror("cd : expected argument");
        }
        else{
            if(chdir(tokens[1])!= 0){
                perror("cd failed");
            }
        }
        return 1;
    }
    else if(strcmp(tokens[0],"exec")==0){
        if(tokens[1] == NULL){
            fprintf(stderr, "exec: expected command\n");
            return 1;
        }
        
        char** args = &tokens[1];
        
        if(execvp(args[0], args) == -1){
            perror("exec failed");
            exit(EXIT_FAILURE);
        }
        return 1;
    }
    return 0;

}