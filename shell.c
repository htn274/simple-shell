#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <readline/readline.h>


#define MAXLINE 80
#define MAXARGS 40

#define RIN_FLAG 1
#define ROUT_FLAG 2

int parseCmd(char* input, char* arg[])
{
    int i;
    for (i = 0; i < MAXARGS; i++) {
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
    //char *str;
    while (i < len) {
        his[i] = (char*)malloc((strlen(arg[i]) + 1) * sizeof(char));
        strcpy(his[i], arg[i]);
        i++;
    }

    for (int j = len; j < MAXARGS; j++) {
        if (his[j] == NULL)
            break;
        his[j] = NULL;
    }
}

pid_t fexecCmd(char **args, const int fd[2])
{
    pid_t p = vfork();
    if (p < 0)
        return -1;

    if (p == 0) {
        if (fd[0] >= 0) {
            if (dup2(fd[0], STDIN_FILENO) < 0)
                exit(1);
        }

        if (fd[1] >= 0) {
            if (dup2(fd[1], STDOUT_FILENO) < 0)
                exit(1);           
        }

        close(fd[0]);
        close(fd[1]);
        execvp(args[0], args);
        exit(1); //error
    } else {
        return p;
    }
}

void execCmd(char **args) {
    const int fd[2] = {-1, -1};
    waitpid(fexecCmd(args, fd), NULL, 0);    
}

void execPipe(char **pipeWriter, char **pipeReader)
{
    // 0: read, 1: write
    int fd[2];

    if (pipe(fd) < 0)
        return;

    int fd0[2] = { fd[0], -1};
    int fd1[2] = { -1, fd[1]};

    pid_t p1 = fexecCmd(pipeWriter,  fd1);
    pid_t p2 = fexecCmd(pipeReader,  fd0);

    close(fd[0]);
    close(fd[1]);

    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
}


void execRedir(char **args, const char * const filename[2], int type)
{
    int fd[2] = {-1, -1};
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    if (type & RIN_FLAG)
    {
        fd[0] = open(filename[0], O_RDONLY, mode);
        if (fd[0] < 0)
        {
            perror("Redirect Input Error");
            return;
        }
    }
    
    if (type & ROUT_FLAG)
    {
        fd[1] = open(filename[1], O_WRONLY | O_CREAT, mode);
        if (fd[1] < 0)
        {
            perror("Redirect Output Error");
            return;
        }
    }

    waitpid(fexecCmd(args, fd), NULL, 0);

    close(fd[0]);
    close(fd[1]);
}


void printTest(char* arg[])
{
    int i = 0;
    while (arg[i] != NULL && i < MAXARGS)
    {
        printf("%s<space>", arg[i]);
        i++;
    }
    printf("\n");
}

void testPipe()
{
    char *first, *second;
    first = readline("first: ");
    fsync(STDOUT_FILENO);
    second = readline("second: ");
    char *arg1[MAXARGS], *arg2[MAXARGS];

    parseCmd(first, arg1);
    parseCmd(second, arg2);

    execPipe(arg1, arg2);
    printf("Test pipe: sucessful\n");
}

void testRedirect()
{
    char *cmd, *filename[2] = {NULL, NULL};
    int type;
    cmd = readline("redir: ");
    printf("type redirect: ");
    scanf("%d", &type);
    filename[type] = readline("file name: ");

    char* args[MAXARGS];
    parseCmd(cmd, args);
    printTest(args);
    execRedir(args, filename, type + 1);
    printf("Test Redirection: sucessful\n");
}

int main()
{
    char *arg[MAXARGS], *history[MAXARGS];
    char *input;
    int numberArg;

    
    while (1)
    {
        testPipe();
        //testRedirect();
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
