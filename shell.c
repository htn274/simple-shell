#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include "command.h"
#include <unistd.h>


#define SHELL_NAME "\x1B[38;5;2m\x1B[1mosh>\x1B[0m "

int running = 0;

void handler(int sig) {
    fputs("\n", stdout);

    if (!running) {
        fputs(SHELL_NAME, stdout);
    }
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
    int i = 0, j = 0;
    for (; s[i]; ++i) {
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

char *replace_env_var(char *s) {
    char *new = calloc(1, sizeof(char)); //empty string
    int i = 0, j =0;;

    for (; s[i]; ++i) {
        if (s[i] == '$') {
            int k = i + 1;
            while (s[k] && isalnum(s[k])) {
                ++k;
            }

            if (k > i + 1) {
                char temp = s[k];
                s[k] = '\0';
                char *val = getenv(s + i + 1);
                s[k] = temp;

                if (val) {
                    int lval = strlen(val);
                    new = realloc(new, (j + lval + 1) * sizeof(char));
                    strcpy(new + j, val);
                    j += lval;
                }

                i = k - 1;
                continue;
            }
        }

        new = realloc(new, (j + 2) * sizeof(char));
        new[j] = s[i];
        ++j;
    }

    new[j] = '\0';
    return new;
}

int main()
{
    signal(SIGINT,handler);
    
    char *s;
    while (1)
    {
        char * wd = getcwd(NULL, 1024);
        fputs("\x1B[38;5;3m", stdout);
        fputs(wd, stdout);
        fputs("\x1B[0m\n", stdout);
        free(wd);

        running = 0;
        s = readline(SHELL_NAME);
        running = 1;

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

        char *s2 = replace_env_var(s);
        struct lcommand c = parse_command(s2);
        free(s2);

        if (c.n > 0) {
            add_history(s);
            exec_command(c);
            free_command(c);
        }

        free(s);
    }
    
    return 0;
}
