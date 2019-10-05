#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include "command.h"

#define SHELL_NAME "\x1B[38;5;2m\x1B[1mosh>\x1B[0m "

int running = 0;

void handler(int sig)
{
    fputs("\n", stdout);
    if (!running)
        fputs(SHELL_NAME, stdout);       
}

int normalize(char **cmd) {
    if (**cmd == '\0')
        return 0;

    char *prev;
    int n = 0;

    
    if (history_length == 0)
        prev = NULL;
    else {
        prev = history_get(history_length)->line;
        n = strlen(prev);
    }
    char *s = *cmd;
    int rep = 0;

    char *new = NULL;
    int j = 0;

    for (int i = 0; s[i]; ++i) {
        if (s[i] == '!' && s[i+1] == '!') {
            if (!prev)
                return -1;

            new = realloc(new, (j + n + 2) * sizeof(char));
            strcpy(new + j, prev);
            j+=n;
            ++i;

            rep = 1;
        } else {
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


int main()
{
    signal(SIGINT,handler);
    
    char *s;
    while (1)
    {
        s = readline(SHELL_NAME);
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

        char *ss = malloc((strlen(s) + 1) * sizeof(char));
        strcpy(ss, s);

        struct command *c = parse_command(s);

        if (c) {
            add_history(ss);
            exec_command(c);
            free_command(c);
        }

        free(ss);
        free(s);
    }
    
    return 0;
}
