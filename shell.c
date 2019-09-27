#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include <readline/readline.h>
#include <readline/history.h>

#define MAXLINE 80
#define MAXARGS 40

void parseCmd(char* input, char* arg[])
{
    int i;
    for (i = 0; i < MAXARGS; i++)
    {
        arg[i] = strsep(&input, " ");
        if (arg[i] == NULL) 
            break;
        if (strlen(arg[i]) == 0)
            i--;
    }
}

void execCmd(char* arg[])
{
    //Create a child process
    pid_t pid = fork();
    if (pid == -1)
    {
        printf("Create child process: failed\n");
        return;
    }
    if (pid == 0)
    {
        if (execvp(arg[0], arg) < 0)
        {
            printf("Could not execute command...\n");
        }
        exit(0);
    }
    else
    {
        wait(NULL);
        return;
    }
}

void printTest(char* arg[])
{
    int i = 0;
    while (arg[i] != NULL && i < MAXARGS)
    {
        printf("%s ", arg[i]);
        i++;
    }
}

int main()
{
    char *arg[MAXARGS];
    char *input;
    while (1){
        input = readline("osh> "); 
        parseCmd(input, arg);
        //printTest(arg);        
        execCmd(arg);
    }
    return 0;
}
