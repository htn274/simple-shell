#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "command.h"

int shell_exit(char **args) {
    if (args[1] && args[2]) {
        fputs("exit error: Too many arguments\n", stderr);
        return -1;
    }
    
    if (args[1])
        exit(atoi(args[1]));
    else
        exit(0);
}

int cd(char **args) {
    int res = 0;
    if (!args[1])
        res = chdir(getenv("HOME"));
    else if (args[2])
        fputs("cd error: Too many arguments\n", stderr);
    else  {
        if (args[1][0] == '~') {
            res = chdir(getenv("HOME"));
            args[1][0] = '.';
        }
        
        if (res >= 0)
            res = chdir(args[1]);
    }
    if (res < 0)
        perror("cd error");

    return res;
}

int set(char **args) {
    if (!args[1] || !args[2]){
        fputs("set error: Too few arguments\n", stderr);
        return -1;
    }
    if (args[3]) {
        fputs("set error: Too many arguments\n", stderr);
        return -1;
    }

    if (setenv(args[1], args[2], 1) < 0) {
        perror("set error");
        return -1;
    }
    return 0;
}

int unset(char **args) {
    if (!args[1]){
        fputs("unset error: Too few arguments\n", stderr);
        return -1;
    }
    if (args[2]) {
        fputs("unset error: Too many arguments\n", stderr);
        return -1;
    }

    if (unsetenv(args[1]) < 0) {
        perror("unset error");
        return -1;
    }

    return 0;
}

void extend_cmd(struct command *c){
    if (strcmp(c->args[0], "ls")   == 0 ||
        strcmp(c->args[0], "grep") == 0) 
    {
        int i;
        ++c->argc;
        c->args = realloc(c->args, (c->argc + 1) * sizeof(char *));
        for (i = c->argc; i > 1; --i)
            c->args[i] = c->args[i-1];
    
        c->args[1] = strdup("--color=tty");
    }
}

pid_t fexec_cmd(char **args, const int fd[3])
{
    if (strcmp(args[0], "exit") == 0) {
        shell_exit(args);
        return -1;
    } else if (strcmp(args[0], "cd") == 0) {
        cd(args);
        return -1;
    } else if (strcmp(args[0], "set") == 0) {
        set(args);
        return -1;
    } else if (strcmp(args[0], "unset") == 0) {
        unset(args);
        return -1;
    }

    pid_t p = vfork();
    if (p < 0) {
        perror("Exec fork failed");
        return -1;
    }

    if (p == 0) {
        if (fd[0] >= 0) {
            if (dup2(fd[0], STDIN_FILENO) < 0) {
                perror("Exec input redirect failed");
                exit(1);
            }
            close(fd[0]);
        }

        if (fd[1] >= 0) {
            if (dup2(fd[1], STDOUT_FILENO) < 0) {
                perror("Exec output redirect failed");
                exit(1);     
            }
            close(fd[1]);
        }

        if (fd[2] >= 0) {
            if (dup2(fd[2], STDERR_FILENO) < 0) {
                perror("Exec output redirect failed");
                exit(1);     
            }
            close(fd[1]);
        }
    
        execvp(args[0], args);
        perror("Exec failed");
        exit(1); //error
    } else {
        if (fd[0] >= 0)
            close(fd[0]);
        
        if (fd[1] >= 0)
            close(fd[1]);

        return p;
    }
}

int redir_in(int *fd, char *file) {
    if (*fd >= 0) {
        fputs("Can't have two input\n", stderr);
        return -1;
    }

    *fd = open(file, O_RDONLY, 0);
    if (*fd < 0)
    {
        perror("Open file to read failed");
        return -1;
    }

    return 0;
}

int redir_out(int *fd, char *file) {
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if (*fd >= 0) {
        fputs("Can't have two output\n", stderr);
        return -1;
    }

    *fd = open(file, O_WRONLY | O_CREAT, mode);
    if (*fd < 0)
    {
        perror("Open file to write failed");
        return -1;
    }
    return 0;
}

int pipe_next(int cfd[3], int nfd[3]) {
    if (cfd[1] >= 0) {
        fputs("Can't have two output\n", stderr);
        return -1;
    }

    if (nfd[0] >= 0) {
        fputs("Can't have two input\n", stderr);
        return -1;
    }

    int pfd[2];
    if (pipe(pfd) < 0) {
        perror("Pipe failed");
        return -1;
    }

    cfd[1] = pfd[1];
    nfd[0] = pfd[0]; 
    return 0;
}

void move_fd(int cfd[3], int nfd[3]) {
    cfd[0] = nfd[0];
    cfd[1] = nfd[1];
    cfd[2] = nfd[2];

    nfd[0] = nfd[1] = nfd[2] = -1;
}


int exec_lcommand(struct lcommand cmd) {
    int cfd[3] = {-1, -1, -1};
    int nfd[3] = {-1, -1, -1};

    pid_t *pid = malloc(cmd.n * sizeof(pid_t));

    int i;
    for (i = 0; i < cmd.n; ++i) {
        struct command *p = cmd.c + i;
        if (p->filename[0] && redir_in(&cfd[0], p->filename[0]) < 0) {
            free(pid);
            return -1;
        }

        if (p->filename[1] && redir_out(&cfd[1], p->filename[1]) < 0) {
            free(pid);
            return -1;
        }

        if (p->filename[2] && redir_out(&cfd[2], p->filename[2]) < 0) {
            free(pid);
            return -1;
        }

        if (p->pipe && pipe_next(cfd, nfd) < 0) {
            free(pid);
            return -1;
        }

        extend_cmd(p);
        pid[i] = fexec_cmd(p->args, cfd);

        if (!p->pipe && !p->async && pid[i] >= 0)
            waitpid(pid[i], NULL, 0);

        move_fd(cfd, nfd);
    }

    for (i = 0; i < cmd.n; ++i)
        if (cmd.c[i].pipe && !cmd.c[i].async && pid[i] >= 0)
            waitpid(pid[i], NULL, 0);

    free(pid);

    return 0;
}
