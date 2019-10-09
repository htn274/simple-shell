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
#include "builtin.h"

struct ljob_t job_table = {0,NULL};


void sigchild_handler()
{
    int new_prompt = 0;
    int i;
    for (i = 0; i < job_table.cap; ++i)
        if (job_table.job[i].running) {
            struct job_t *job = &job_table.job[i];
            if (waitpid(job->pid, NULL, WNOHANG) > 0) {
                job->running = 0;
                printf("\n[%d]\t%d done\t%s\n", i + 1, job->pid, job->cmd);
                free(job->cmd);
                new_prompt = 1;
            }
        }

    if (new_prompt) {
        print_wd();
        interrupt_readline();
    }
    
}

int fexec_cmd(struct command_t *cmd, const int fd[3], pid_t *pid, int p_stat)
{
    char **args = cmd->args;

    if (exec_builtin(args, NULL)) {
        return 0;
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

        if (p->pipe && !p->async && pid >= 0) {
            int job_id = add_job(&pipe_job);
            pipe_job.job[job_id].pid = pid;
        }

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
