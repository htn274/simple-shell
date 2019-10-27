#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <unistd.h>

#include "command.h"
#include "error.h"
#include "shell.h"
#include "alias.h"
#include "read.h"

#define PATH_MAX 1024

void print_wd() {
    char * wd = getcwd(NULL, PATH_MAX);
    fputs("\x1B[38;5;3m", stdout);
    fputs(wd, stdout);
    fputs("\x1B[0m\n", stdout);
    free(wd);
}

void print_prompt()
{
    print_wd();
    fputs(SHELL_NAME, stdout);
    fflush(stdout);
}

int running = 0;

void interrupt_readline()
{
    rl_on_new_line(); // Regenerate the prompt on a newline
    rl_redisplay();
}

void handler(int sig) {
    if (!running) {
        puts("");
        rl_replace_line("", 0); // Clear the previous text
        interrupt_readline();
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



int main()
{
    signal(SIGINT, handler);
    signal(SIGCHLD, sigchild_handler);
    
    set_alias("ls", "ls --color=auto");
    set_alias("grep", "grep --color=auto");
    set_alias("l", "ls -lah");
    set_alias("la", "ls -lAh");

    char *s;
    while (1)
    {
        print_prompt();
        running = 0;
        s = read_cmd();
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
