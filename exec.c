#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "error.h"
#include "command.h"
#include "job.h"
#include "shell.h"
#include "builtin.h"
#include "read.h"

#define _close(fd) {if(fd >= 0) close(fd);}

void sigchild_handler()
{
    int new_prompt = 0;
    int i;
    for (i = 0; i < bg_job_table.cap; ++i)
        if (bg_job_table.job[i].running) {
            struct job_t *job = &bg_job_table.job[i];
            int code;
            if (waitpid(job->pid, &code, WNOHANG | WUNTRACED | WCONTINUED) < 0)
                continue;

            if (WIFSTOPPED(code) && job->running == 1) {
                job->running = 2;
                new_line();
                printf("[%d]\t%d suspended", i + 1, job->pid);
                
                if (WSTOPSIG(code) == SIGTTIN)
                    printf("%s", " (tty input)");
                else if(WSTOPSIG(code) == SIGTTOU)
                    printf("%s", " (tty output)");

                printf("\t%s\n", job->cmd);

                free(job->cmd);
                new_prompt = 1;
            }

            if (WIFEXITED(code)) {
                job->running = 0;
                new_line();
                printf("[%d]\t%d done\t%s\n", i + 1, job->pid, job->cmd);
                free(job->cmd);
                new_prompt = 1;
            }
        }

    if (new_prompt) {
        print_prompt();
        //clear_buffer();
    }
    
}

void block_chld() {
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, SIGCHLD);
    sigprocmask(SIG_BLOCK, &s, NULL);
}

void unblock_chld() {
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, SIGCHLD);
    sigprocmask(SIG_UNBLOCK, &s, NULL);
}

int pipe_next(fd_list fd) {
    if (fd[1] >= 0) {
        fputs("Can't have two output\n", stderr);
        return -1;
    }

    int pfd[2];
    if (pipe(pfd) < 0) {
        perror("Pipe failed");
        return -1;
    }

    fd[1] = pfd[1];
    return pfd[0];
}

void close_fd(fd_list fd) {
    _close(fd[0]);
    _close(fd[1]);
    _close(fd[2]);
}

//nfd is read pipe out file description// need to close in the child process 
int fexec_cmd(struct command_t *cmd, fd_list fd, int *nfd, struct job_t *job)
{
    char **args = cmd->args;

    if (cmd->pipe) {
        if (*nfd >= 0) {
            fputs("Can't both pipe and redirect\n", stderr);
            return -1;
        }
        if ((*nfd = pipe_next(fd)) < 0)
            return -1;
    }

    if (exec_builtin(args, NULL, fd)) {
        if (job) {
            job->pid = -1;
            job->cmd = NULL;
            job->running = 0;
        }
        
        return 0;
    }


    block_chld();

    pid_t p = fork();
    if (p < 0) {
        perror("Exec fork failed");
        job->pid = -1;
        return -1;
    }

    if (p == 0) {
        //reset signal
        signal(SIGINT, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        if (fd[0] >= 0 && dup2(fd[0], STDIN_FILENO) < 0) {
            perror("Exec input redirect failed");
            _exit(1);
        }

        if (fd[1] >= 0 && dup2(fd[1], STDOUT_FILENO) < 0) {
            perror("Exec output redirect failed");
            _exit(1);     
        }

        if (fd[2] >= 0 && dup2(fd[2], STDERR_FILENO) < 0) {
            perror("Exec error output redirect failed");
            _exit(1);     
        }
        
        _close(*nfd);
        close_fd(fd);

        unblock_chld();

        execvp(args[0], args);
        if (errno == ENOENT)
            fprintf(stderr, "Exec failed: command not found (%s)\n", args[0]);
        else
            perror("Exec failed");

        _exit(1); //error
    } else {

        if (setpgid(p, p) < 0)
            perror("Detach failed"); //detach from parent process group

        if (!cmd->async && !cmd->pipe) {
            signal(SIGTTOU, SIG_IGN);
            signal(SIGTTIN, SIG_IGN);
            signal(SIGTSTP, SIG_IGN);

            tcsetpgrp(STDIN_FILENO, getpgid(p));
        } else {
            tcsetpgrp(STDIN_FILENO, getpgrp());

            signal(SIGTTOU, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
        }

        close_fd(fd);

        if (job) {
            job->pid = p;
            job->running = 1;
            job->cmd = cmd_to_string(cmd);
        }

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

    *fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (*fd < 0)
    {
        perror("Open file to write failed");
        return -1;
    }
    return 0;
}


void move_fd(fd_list cfd, fd_list nfd) {
    cfd[0] = nfd[0];
    cfd[1] = nfd[1];
    cfd[2] = nfd[2];

    nfd[0] = nfd[1] = nfd[2] = -1;
}


int exec_lcommand(struct lcommand_t cmd) {
    fd_list cfd = {-1, -1, -1};
    fd_list nfd = {-1, -1, -1};

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

        struct job_t job = null_job;

        fexec_cmd(p, cfd, &nfd[0], &job);
        move_fd(cfd, nfd);

        if (job.pid < 0) {
            if (p->pipe)
                break;
            else
                continue;
        }

        if (bg_stat) {
            int job_id = get_empty_job(&bg_job_table);
            printf("[%d] %d\n", job_id + 1, job.pid);
            bg_job_table.job[job_id] = job_dup(job);
        }

        unblock_chld();

        if (!p->async) {
            if (!p->pipe) {
                waitpid(job.pid, NULL, 0);

                if (tcsetpgrp(STDOUT_FILENO, getpgrp()) < 0)
                    perror("set input foreground failed");

            } else {
                int job_id = add_job(&pipe_job);
                pipe_job.job[job_id] = job_dup(job);
            }
        }

        free_job(&job);
    }

    for (i = 0; i < pipe_job.cap; ++i)
        waitpid(pipe_job.job[i].pid, NULL, 0);

    tcsetpgrp(STDIN_FILENO, getpgrp());

    signal(SIGTTOU, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);

    free_ljob(&pipe_job);
    return 0;
exec_fail:
    close_fd(cfd);
    close_fd(nfd);
    free_ljob(&pipe_job);
    return -1;
}
