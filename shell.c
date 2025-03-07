#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>

// Function prototypes
void cash();
char** splitinput(char *buff);
int executecmd(char** tokens);
int checkAndExecuteInbuiltFunctions(char** tokens);
char** getCommands(char* buff);

struct Command
{
    char *command;
    char *operator;
};



void cash(){
    while(1){
        printf("$");
        fflush(stdout);
        char buff[1024];
        if(fgets(buff,1024,stdin)==NULL)
            exit(EXIT_SUCCESS);
        
        char** commands=getCommands(buff);
        int i = 0;
        while(commands[i]!=NULL){
            printf("Command :%s",commands[i]);
            char** tokens=splitinput(commands[i]);
            int nextCmd;
            if(tokens!=NULL){
                nextCmd = executecmd(tokens);
            }
            free(tokens);
            i++;
        }
        free(commands);
        
    }
}

int main(int argc,char **argv){
    cash();
    return EXIT_SUCCESS;
}


char ** getCommands(char *buff){
    int position=0;
    char **commands = malloc(1024* sizeof(char*));
    char* com;

    buff[strcspn(buff, "\n")] = 0;
    com = strtok(buff,";");
    while(com!=NULL){
        commands[position] = com;
        position++;
        com = strtok(NULL,";");
    }
    commands[position]=NULL;
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
        }
        exit(EXIT_FAILURE);
    }
    else{
        waitpid(pid,NULL,0);
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
    return 0;

}