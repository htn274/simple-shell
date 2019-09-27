#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include <readline/readline.h>

#define MAXLINE 80
#define MAXARGS 40


int parseCmd(char* input, char* arg[])
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
    return i;
}

void addHistory(char *arg[], int len, char *his[])
{
    int i = 0;
    char *str;
    while (i < len)
    {
        his[i] = (char*)malloc((strlen(arg[i]) + 1) * sizeof(char));
        strcpy(his[i], arg[i]);
        //printf("%s %s\n", arg[i], his[i]);
        i++;
    }
    for (int j = len; j < MAXARGS; j++)
    {
        if (his[j] == NULL)
            break;
        his[j] = NULL;
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
    printf("\n");
}

int main()
{
    char *arg[MAXARGS], *history[MAXARGS];
    char *input;
    int numberArg;
    while (1){
        input = readline("osh> "); 
        numberArg = parseCmd(input, arg);
        //printTest(arg);
        if (strcmp(arg[0], "!!") == 0)
        {
            if (history[0] == NULL)
            {
                printf("No history command!\n");
            }
            else
            {
                printf("osh> ");
                printTest(history);
                execCmd(history);
            }
        }
        else
        {
           addHistory(arg, numberArg, history);
           execCmd(arg);
        }
    }
    return 0;
}
