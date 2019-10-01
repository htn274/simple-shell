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
        perror("Error");
        //printf("Create child process: failed\n");
        return;
    }
    if (pid == 0)
    {
        if (execvp(arg[0], arg) < 0)
        {
            perror("Command error");
            //printf("Could not execute command...\n");
        }
        exit(0);
    }
    else
    {
        wait(NULL);
        return;
    }
}

void execPipe(char* toPipe[], char* fromPipe[])
{
    // 0: read, 1: write
    int fd[2];
    pid_t p1, p2;

    if (pipe(fd) < 0)
    {
        perror("Initialized pipe error");
        return;
    }

    printf("Fd: %d %d\n", fd[0], fd[1]);

    fflush(stdout);
    p1 = fork();

    if (p1 < 0)
    {
        perror("Pipe error");
        return;
    }

    if (p1 == 0)
    {
        printf("P1\n");
        // p1 write the result to the pipe
        close(fd[0]);
        if (dup2(fd[1], STDOUT_FILENO) < 0)
        {
            perror("Pipe error");
            exit(0);
        }
        close(fd[1]);

       printf("P1\n");
       if (execvp(toPipe[0], toPipe) < 0)
       {
           perror("Pipe error");
           exit(0);
       }
       printf("P1 is executed!");
    }
    else
    {
        // run p2
        p2 = fork();

        if (p2 < 0)
        {
            perror("Pipe error");
            return;
        }

        if (p2 == 0)
        {
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);

            if (execvp(fromPipe[0], fromPipe) < 0)
            {
                perror("Pipe error");
                exit(0);
            }
            printf("P2 is executed!");
        }
        else
        {
            wait(NULL);
            wait(NULL);
        }
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

void testPipe()
{
    char *first, *second;
    first = readline("first: ");
    fflush(stdin);
    second = readline("second: ");
    char *arg1[MAXARGS], arg2[MAXARGS];

    int num = parseCmd(first, arg1);
    num = parseCmd(second, arg2);

    execPipe(arg1, arg2);
    //printTest(arg1);
    //printTest(arg2);
    printf("Test pipe: sucessful");
}

int main()
{
    char *arg[MAXARGS], *history[MAXARGS];
    char *input;
    int numberArg;

    testPipe();
    /*
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
    */
    return 0;
}
