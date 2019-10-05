#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>

#define SHELL_NAME "\x1B[38;5;2m\x1B[1mosh>\x1B[0m "

int running = 0;

void handler(int sig)
{
    fputs("\n", stdout);
    if (!running)
        fputs(SHELL_NAME, stdout);
        
}

struct command {
    char **args;
    int async;

    char *filename[2];
    int pipe; //true mean pipe, when pipe to next command

    struct command *next;
};

pid_t fexecCmd(char **args, const int fd[2])
{
    pid_t p = vfork();
    if (p < 0)
        return -1;

    if (p == 0) {
        if (fd[0] >= 0) {
            if (dup2(fd[0], STDIN_FILENO) < 0) {
                perror("Redirect in fail");
                exit(1);
            }
            close(fd[0]);
        }

        if (fd[1] >= 0) {
            if (dup2(fd[1], STDOUT_FILENO) < 0) {
                perror("Redirect out fail");
                exit(1);     
            }
            close(fd[1]);
        }
    
        execvp(args[0], args);
        perror("Exec cmd fail");
        exit(1); //error
    } else {
        if (fd[0] >= 0)
            close(fd[0]);
        
        if (fd[1] >= 0)
            close(fd[1]);

        return p;
    }
}

int redir_in(int fd[2], char *file) {
    if (fd[0] >= 0) {
        fputs("Can't have two input\n", stderr);
        return -1;
    }

    fd[0] = open(file, O_RDONLY, 0);
    if (fd[0] < 0)
    {
        perror("Redirect Input Error");
        return -1;
    }

    return 0;
}

int redir_out(int fd[2], char *file) {
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if (fd[1] >= 0) {
        fputs("Can't have two output\n", stderr);
        return -1;
    }

    fd[1] = open(file, O_WRONLY | O_CREAT, mode);
    if (fd[1] < 0)
    {
        perror("Redirect Output Error");
        return -1;
    }
    return 0;
}

int pipe_next(int cfd[2], int nfd[2]) {
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
        perror("Pipe error");
        return -1;
    }

    cfd[1] = pfd[1];
    nfd[0] = pfd[0]; 
    return 0;
}

void move_fd(int cfd[2], int nfd[2]) {
    cfd[0] = nfd[0];
    cfd[1] = nfd[1];

    nfd[0] = nfd[1] = -1;
}


int run_command(struct command *cmd) {
    running = 1;

    int cfd[2] = {-1, -1};
    int nfd[2] = {-1, -1};

    struct command *p = cmd;

    while (p) {
        if (p->filename[0])
            redir_in(cfd, p->filename[0]);

        if (p->filename[1])
            redir_out(cfd, p->filename[1]);

        if (p->pipe)
            pipe_next(cfd, nfd);

        pid_t pid = fexecCmd(p->args, cfd);

        if (!p->async) {
            waitpid(pid, NULL, 0);
        }

        move_fd(cfd, nfd);
        p = p->next;
    }

    running = 0;

    return 0;
}

void skip_space(char *s, int *x) {
    int i = *x;

    while (s[i] && isspace(s[i]))
        ++i;

    *x = i;
}

int split(char* s, int *x)
{
    int i = *x;
    int j = i;

    char delim = ' ';

    int plain = 1;

    for (;s[i]; ++i) {
        if (delim != ' ') {
            if (s[i] == delim) 
                delim = ' ';
            else
                s[j++] = s[i];
            
        } else {
            if (isspace(s[i])) {
                s[j++] = '\0';
                ++i;
                break;
            } else if (s[i] == '"' || s[i] == '\'') {
                //meo bad for operator
                plain = 0;
                delim = s[i];
            } else {
                s[j++] = s[i];
            }
        }
    };

    if (!s[i])
        s[j++] = '\0';

    *x = i;

    skip_space(s, x);

    return plain;
}

void free_command(struct command *c) {
    if (!c)
        return;

    free(c->args);
    free_command(c->next);
    free(c);
}

struct command *parseCmd(char *cmd) {

    int i = 0;
    int pos = 0;
    int argc = 0;

    skip_space(cmd, &pos);

    if (cmd[pos] == '\0')
        return NULL;

    struct command *c = calloc(1, sizeof(struct command));
    struct command *head = c;
    struct command *prev = NULL;

    do {
        i = pos;

        int plain = split(cmd, &pos);

        int is_arg = !plain;
        int is_done = 0;

        if (plain) {
            if (cmd[i] == '\0') {
                is_arg = 0;
                is_done = 1;
            } else if (strcmp(cmd + i, "|") == 0) {
                c->pipe = 1;
                is_done = 1;
            } else if (strcmp(cmd + i, "&&") == 0) {
                is_done = 1;
            } else if (strcmp(cmd + i, "&") == 0) {
                c->async = 1;
            } else if (strcmp(cmd + i, "<") == 0) {
                int j = pos;
                split(cmd, &pos); //this is file :P
                c->filename[0] = cmd + j;
            } else if (strcmp(cmd + i, ">") == 0) {
                int j = pos;
                split(cmd, &pos); //this is file :P
                c->filename[1] = cmd + j;
            } else {
                is_arg = 1;
            }
        }

        if (is_arg){
            ++argc;
            c->args = realloc(c->args, sizeof(char*) * (argc + 1));
            c->args[argc - 1] = cmd + i;
        }

        if (is_done) {
            if (!c->args) {
                fputs("Command with no arguments\n", stderr);
                //free here
                free_command(head);
                return NULL;
            }

            c->args[argc] = NULL;
            argc = 0;
            //add to head
            c->next = calloc(1, sizeof(struct command));
            prev = c;
            c = c->next;
        }
    } while(cmd[i] != '\0');

    free(c);
    prev->next = NULL;
    
    return head;
}

int main()
{
    signal(SIGINT,handler);
    
    char *s;
    while (1)
    {
        s = readline(SHELL_NAME);
        if (!s)
            break;

        add_history(s);
        
        struct command *c = parseCmd(s);
        run_command(c);
        free_command(c);
        free(s);
    }
    
    return 0;
}
