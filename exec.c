#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "error.h"

#include "command.h"
#include "job.h"
#include "shell.h"

struct ljob_t job_table = {0,NULL};


void sigchild_handler(int sig)
{
    int new_prompt = 0;
    int i;
    for (i = 0; i < job_table.cap; ++i)
        if (job_table.job[i].running) {
            struct job_t *job = &job_table.job[i];
            if (waitpid(job->pid, NULL, WNOHANG) >= 0) {
                job->running = 0;
                printf("\n[%d]\t%d done\t%s\n", i + 1, job->pid, job->cmd);
                free(job->cmd);
                new_prompt = 1;
            }
        }

    if (new_prompt)
        print_prompt();
    
}

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

int alias(char **args) {
    if (!args[1]){
        fputs("alias error: Too few arguments\n", stderr);
        return -1;
    } else if (!args[2]) {
        const char *cmd = find_alias(args[1]);
        if (cmd)
            printf("alias %s='%s'\n", args[1], cmd);
        else
            printf("alias %s is not exist\n", args[1]);
        return 0;
    } else if(args[3]) {
        fputs("alias error: Too many arguments\n", stderr);
        return -1;
    } else {
        if (set_alias(args[1], args[2]) < 0) {
            fputs(error_str, stderr);
            return -1;
        }
        return 0;
    }
}

int unalias(char **args) {
    if (!args[1]){
        fputs("alias error: Too few arguments\n", stderr);
        return -1;
    } else if (args[2]) {
        fputs("alias error: Too many arguments\n", stderr);
        return -1;
    } else {    
        if (unset_alias(args[1]) < 0) {
            printf("alias %s is not exist\n", args[1]);
            return -1;
        }
        return 0;
    }
}

int fexec_cmd(struct command_t *cmd, const int fd[3], pid_t *pid, int p_stat)
{
    char **args = cmd->args;
    if (strcmp(args[0], "exit") == 0) {
        return shell_exit(args);
    } else if (strcmp(args[0], "cd") == 0) {
        return cd(args);
    } else if (strcmp(args[0], "set") == 0) {
        return set(args);
    } else if (strcmp(args[0], "unset") == 0) {
        return unset(args);
    } else if (strcmp(args[0], "alias") == 0) {
        return alias(args);
    } else if (strcmp(args[0], "unalias") == 0) {
        return unalias(args);
    }

    int pfd[2]; //pipe for communicate between child and parent
    if (pipe(pfd) < 0)
        perror("Exec pipe failed");

    pid_t p = fork();
    if (p < 0) {
        perror("Exec fork failed");
        return -1;
    }

    if (p == 0) {
        if (fd[0] >= 0) {
            if (dup2(fd[0], STDIN_FILENO) < 0) {
                perror("Exec input redirect failed");
                _exit(1);
            }
            close(fd[0]);
        }

        if (fd[1] >= 0) {
            if (dup2(fd[1], STDOUT_FILENO) < 0) {
                perror("Exec output redirect failed");
                _exit(1);     
            }
            close(fd[1]);
        }

        if (fd[2] >= 0) {
            if (dup2(fd[2], STDERR_FILENO) < 0) {
                perror("Exec error output redirect failed");
                _exit(1);     
            }
            close(fd[1]);
        }

        //read til eof
        char tmp;
        close(pfd[1]);
        if (read(pfd[0], &tmp, 1) != 0) {
            fprintf(stderr, "Exec failed: pipe error\n");
            _exit(1);
        }

        execvp(args[0], args);
        if (errno == ENOENT)
            fprintf(stderr, "Exec failed: command not found (%s)\n", args[0]);
        else
            perror("Exec failed");

        _exit(1); //error
    } else {
        if (fd[0] >= 0)
            close(fd[0]);
        
        if (fd[1] >= 0)
            close(fd[1]);

        if (p_stat) {
            int job_id = get_empty_job(&job_table);
            printf("[%d] %d\n", job_id + 1, p);
            job_table.job[job_id].pid = p;
            job_table.job[job_id].running = 1;
            job_table.job[job_id].cmd = cmd_to_string(cmd);
        }

        close(pfd[0]);
        close(pfd[1]);

        *pid = p;

        return 0;
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


int exec_lcommand(struct lcommand_t cmd) {
    int cfd[3] = {-1, -1, -1};
    int nfd[3] = {-1, -1, -1};

    struct ljob_t pipe_job = null_ljob;

    int bg_stat = (cmd.n == 1) && cmd.c[0].async; //print status

    int i;
    for (i = 0; i < cmd.n; ++i) {
        struct command_t *p = cmd.c + i;
        if (p->filename[0] && redir_in(&cfd[0], p->filename[0]) < 0)
            goto exec_fail;
            
        if (p->filename[1] && redir_out(&cfd[1], p->filename[1]) < 0) 
            goto exec_fail;

        if (p->filename[2] && redir_out(&cfd[2], p->filename[2]) < 0)
            goto exec_fail;

        if (p->pipe && pipe_next(cfd, nfd) < 0)
            goto exec_fail;

        pid_t pid = -1;
        fexec_cmd(p, cfd, &pid, bg_stat);


        if (!p->pipe && !p->async && pid >= 0)
            waitpid(pid, NULL, 0);

        if (p->pipe && !p->async && pid >= 0)
            pipe_job.job[add_job(&pipe_job)].pid = pid;

        move_fd(cfd, nfd);
    }

    for (i = 0; i < pipe_job.cap; ++i)
        waitpid(pipe_job.job[i].pid, NULL, 0);

    free_ljob(&pipe_job);
    return 0;
exec_fail:
    free_ljob(&pipe_job);
    return -1;
}
