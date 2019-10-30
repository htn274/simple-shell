#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "command.h"
#include "error.h"
#include "shell.h"
#include "alias.h"
#include "read.h"
#include "hist.h"

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



int main(int argc, char **argv)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, sigchild_handler);
    
    set_alias("ls", "ls --color=auto");
    set_alias("grep", "grep --color=auto");
    set_alias("l", "ls -lah");
    set_alias("la", "ls -lAh");

    init_read();

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
