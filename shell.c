#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#include "command.h"
#include "error.h"
#include "shell.h"
#include "alias.h"
#include "read.h"
#include "hist.h"
#include "job.h"

void sigchild_handler()
{
    int new_prompt = 0;
    int i;
    for (i = 0; i < bg_job_table.cap; ++i)
        if (bg_job_table.job[i].running != JOB_DONE) {
            struct job_t *job = &bg_job_table.job[i];
            int code;
            if (waitpid(job->pid, &code, WNOHANG | WUNTRACED | WCONTINUED) < 0)
                continue;

            if (WIFSTOPPED(code) && job->running == JOB_RUNNING) {
                job->running = JOB_STOPPED;
                new_line();
                printf("[%d]\t%d suspended", i + 1, job->pid);
                
                if (WSTOPSIG(code) == SIGTTIN)
                    printf("%s", " (tty input)");
                else if(WSTOPSIG(code) == SIGTTOU)
                    printf("%s", " (tty output)");

                printf("\t%s\n", job->cmd);
                new_prompt = 1;
            }

            if (WIFEXITED(code)) {
                job->running = JOB_DONE;
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

char *recent = NULL;

int normalize(char **cmd) {
    if (**cmd == '\0')
        return 0;

    const char *prev;
    int n = 0;
    
    if (!hist_len())
        prev = NULL;
    else {
        prev = get_history(hist_len() - 1);
        n = strlen(prev);
    }

    char *s = *cmd;
    int rep = 0;

    char delim = ' ';

    char *new = NULL;
    int i = 0, j = 0;
    for (; s[i]; ++i) {
        if (delim != '\'' && s[i] == '!' && s[i+1] == '!') {
            if (!prev) {
                free(new);
                return -1;
            }

            new = realloc(new, (j + n + 2) * sizeof(char));
            strcpy(new + j, prev);
            j+=n;
            ++i;

            rep = 1;
        } else {
            if (s[i] == delim)
                delim = ' ';
            else if (delim == ' ' && (s[i] == '"' || s[i] == '\''))
                delim = s[i];

            new = realloc(new, (j + 2) * sizeof(char));
            new[j] = s[i];
            ++j;
        }
    }

    new[j] = '\0';
    free(s);

    *cmd = new;
    return rep;
}

#define RC_FILE ".osh_rc"
#define MAX_LINE 1000

void setup() {
    char *home_path = getenv("HOME");
    int len1 = strlen(home_path);
    int len2 = strlen(RC_FILE);
    char *rc_path = malloc(len1 + 1 + len2 + 1);
    strcpy(rc_path, home_path);
    rc_path[len1] = '/';
    strcpy(rc_path + len1 + 1, RC_FILE);

    FILE *f = fopen(rc_path, "r");

    if (!f)
        return;

    char cmd[MAX_LINE  +1];
    while (!feof(f)) {
        if (fgets(cmd ,MAX_LINE,f)) {
            struct lcommand_t c;
            if (parse_command(cmd, &c))
                fputs(error_str, stderr);

            if (c.n > 0) {
                exec_lcommand(c);
                free_lcommand(&c);
            }
        }
    }

    fclose(f);

    free(rc_path);
}

int main(int argc, char **argv)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, sigchild_handler);

    init_read();

    setup();

    if (argc > 1) {
        //support for -c
        int i;
        printf("Running with args: ");
        for (i = 0; i < argc; ++i)
            printf("%s ", argv[i]);
        printf("\n");

        for (i = 0; i < argc; ++i)
            if (strcmp(argv[i], "-c") == 0) {
                if (i + 1 >= argc) {
                    fputs("Expected command after -c\n", stderr);
                    return 1;
                } else {
                    struct lcommand_t c;
                    if (parse_command(argv[i+1], &c))
                        fputs(error_str, stderr);

                    if (c.n > 0) {
                        exec_lcommand(c);
                        free_lcommand(&c);
                    }
                    exit(0);
                }
            }
    }

    char *s;
    while (1)
    {
        print_prompt();
        s = read_cmd();

        if (!s)
            break;

        int r = normalize(&s);

        if (r == 1) {
            fputs(s, stdout);
            fputs("\n", stdout);
        } else if (r == -1){
            fputs("No history\n", stdout);
            free(s);
            continue;
        }

        struct lcommand_t c;
        if (parse_command(s, &c))
            fputs(error_str, stderr);

        if (c.n > 0) {
            add_history(s);
            exec_lcommand(c);
            free_lcommand(&c);
        }

        free(s);
    }
    
    return 0;
}
