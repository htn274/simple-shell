
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include "builtin.h"
#include "error.h"
#include "alias.h"
#include "hist.h"
#include "job.h"
#include "term.h"

#define PATH_MAX 4096

int exec_builtin(char **args, int *res, fd_list fd)
{
    for (int i = 0; i < bin_cnt; ++i)
        if (strcmp(bin_fun[i].name, args[0]) == 0) {
            //prepare fd
            fflush(stdin);
            fflush(stdout);
            fflush(stderr);
            int fd_in = dup(STDIN_FILENO);
            int fd_out = dup(STDOUT_FILENO);
            int fd_err = dup(STDERR_FILENO);

            if (fd[0] >= 0) {
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]);
            }

            if (fd[1] >= 0) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
            }

            if (fd[2] >= 0) {
                dup2(fd[2], STDERR_FILENO);
                close(fd[2]);
            }

            int r = bin_fun[i].fun(args);

            fflush(stdin);
            fflush(stdout);
            fflush(stderr);
            if (fd[0] >= 0)
                dup2(fd_in, STDIN_FILENO);

            if (fd[1] >= 0)
                dup2(fd_out, STDOUT_FILENO);

            if (fd[2] >= 0)
                dup2(fd_err, STDERR_FILENO);

            close(fd_in);
            close(fd_out);
            close(fd_err);

            if (res)
                *res = r;
            return 1;
        }
    return 0;
}

int is_num(char *s) {
    while(*s) {
        if (!isdigit(*s))
            return 0;
        ++s;
    }
    return 1;
}

int shell_exit(char **args) {
    if (args[1] && args[2]) {
        fputs("exit error: Too many arguments\n", stderr);
        return -1;
    }

    if (args[1] && !is_num(args[1])) {
        fputs("exit error: Exit code should be a non-negative integer\n", stderr);
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
    else 
        res = chdir(args[1]);

    if (res < 0)
        perror("cd error");
    else {
        char * wd = getcwd(NULL, PATH_MAX);
        setenv("PWD", wd, 1);
        free(wd);
    }

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
        int i;
        int n_alias = get_alias_cnt();
        for (i = 0; i < n_alias; ++i) 
            printf("alias %s='%s'\n", get_alias(i), get_alias_cmd(i));
        return 0;
    } else if (!args[2]) {
        const char *cmd = get_alias_cmd(find_alias(args[1]));
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

int history(char **args) {
    if (args[1]){
        fputs("history error: Too many arguments\n", stderr);
        return -1;
    }

    for (int i = 0; i < hist_len(); ++i)
        printf("%d\t%s\n", i + 1, get_history(i));
    
    return 0;
}

int exec(char **args) {
    if (!args[1]) {
        fputs("exec error: Too few arguments\n", stderr);
        return -1;
    }

    return execvp(args[1], args + 1);
}


int fg(char **args) {
    if (!args[1]) {
        fputs("fg error: Too few arguments\n", stderr);
        return -1;
    }

    if (!is_num(args[1])) {
        fputs("fg error: Job id should be a positive integer\n", stderr);
        return -1;
    }
    
    int job_id = atoi(args[1]) - 1;
    struct job_t *job = get_job(&bg_job_table, job_id);

    if (job->running == JOB_DONE) {
        fputs("fg error: Job is not found, or already finished\n", stderr);
        return -1;
    }

    if (job->running == JOB_STOPPED) {
        printf("[%d]\t%d resume\n", job_id + 1, job->pid);
        kill(job->pid, SIGCONT);
    }
    
    set_control(getpgid(job->pid), 0);
    waitpid(job->pid, NULL, 0);
    job->running = JOB_DONE;
    set_control(getpgrp(), 1);
    return 0; //return process exit code ??
}


struct builtin_t bin_fun[] = {{"exit", shell_exit},
                              {"cd", cd},
                              {"set", set},
                              {"unset", unset},
                              {"alias", alias},
                              {"unalias", unalias},
                              {"history", history},
                              {"exec", exec},
                              {"fg", fg}
                              };

int bin_cnt = sizeof(bin_fun)/ sizeof(bin_fun[0]);

