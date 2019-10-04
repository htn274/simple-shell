#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>

#define MAXLINE 80
#define MAXARGS 40

#define IN 0
#define OUT 1


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
        return;
    }
    if (pid == 0)
    {
        if (execvp(arg[0], arg) < 0)
        {
            perror("Command error");
        }
        exit(1);
    }
    else
    {
        wait(NULL);
        return;
    }
}

void execPipe(char* pipeWriter[], char* pipeReader[])
{
    // 0: read, 1: write
    int fd[2];
    pid_t p1, p2;

    if (pipe(fd) < 0)
    {
        perror("Initialized pipe error");
        return;
    }

    p1 = fork();

    if (p1 < 0)
    {
        perror("Pipe error");
        return;
    }

    if (p1 == 0)
    {
        // p1 write the result to the pipe
        //close(fd[0]);
        fflush(stdout);
        if (dup2(fd[1], STDOUT_FILENO) < 0)
        {
            perror("Pipe error");
            exit(1);
        }
        close(fd[1]);
        close(fd[0]);

       if (execvp(pipeWriter[0], pipeWriter) < 0)
       {
           perror("Pipe error");
           exit(1);
       }
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
            //close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            close(fd[1]);

            if (execvp(pipeReader[0], pipeReader) < 0)
            {
                perror("Pipe error");
                exit(1);
            }
        }
        else
        {
            close(fd[0]);
            close(fd[1]);
            wait(NULL);
            wait(NULL);
        }
    }
}


void execRedirection(char *arg[], char* filename, int type)
{
    int fd;
    pid_t pid;
    pid = fork();
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int tmpIN = dup(IN), tmpOUT = dup(OUT);

    if (type == IN)
        {
            fd = open(filename, O_RDONLY, mode);
            if (fd < 0)
            {
                perror("Redirect Input Error");
                exit(1);
            }
            if (dup2(fd, STDIN_FILENO) < 0)
            {
                perror("Redirect Input Error");
                exit(1);
            }
            close(fd);
        }
        else
        {
            fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, mode);
            if (fd < 0)
            {
                perror("Redirect Output Error");
                exit(1);
            }

            if (dup2(fd, STDOUT_FILENO) < 0)
            {
                perror("Redirect Output Error");
                exit(1);
            }
            close(fd);
        }
    if (pid == -1)
    {
        perror("Redirect Error");
        exit(1);
    }

    if (pid == 0)
    {
        if (execvp(arg[0], arg) < 0)
        {
            perror("Redirect Error");
            exit(1);
        }
        close(fd);
    }
    else
    {
        close(fd);
        dup2(tmpIN, IN);
        dup2(tmpOUT, OUT);
        close(tmpIN);
        close(tmpOUT);
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

void testPipe()
{
    char *first, *second;
    first = readline("first: ");
    fflush(stdin);
    second = readline("second: ");
    char *arg1[MAXARGS], *arg2[MAXARGS];

    int num = parseCmd(first, arg1);
    num = parseCmd(second, arg2);

    execPipe(arg1, arg2);
    printf("Test pipe: sucessful\n");
}

void testRedirect()
{
    char *cmd, *filename;
    int type;
    cmd = readline("redir: ");
    filename = readline("file name: ");
    printf("type redirect: ");
    scanf("%d", &type);
    char* arg[MAXARGS];
    parseCmd(cmd, arg);
    execRedirection(arg, filename, type);
    printf("Test Redirection: sucessful\n");
}

int main()
{
    char *arg[MAXARGS], *history[MAXARGS];
    char *input;
    int numberArg;

    
    while (1)
    {
        //testPipe();
        testRedirect();
    }
      
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
