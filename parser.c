#include "command.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

void free_command(struct command *c) {
    if (!c)
        return;

    free(c->args);
    free_command(c->next);
    free(c);
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
                delim = '\0';
                ++i;
                break;
            } else if (s[i] == '"' || s[i] == '\'') {
                plain = 0;
                delim = s[i];
            } else {
                s[j++] = s[i];
            }
        }
    };

    if (delim == ' ' && !s[i]) {
        s[j++] = '\0';
        delim = '\0';
    }
    
    if (delim != '\0')
        return -1; //error

    *x = i;

    skip_space(s, x);

    return plain;
}

struct command *parse_command(char *cmd) {
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

        if (plain == -1) {
            fputs("Parse Error: Not closing \" or '\n", stderr);
            free_command(head);
            return NULL;
        }

        int is_arg = !plain;
        int is_done = 0;

        if (cmd[i] == '\0') {
            is_arg = !plain;
            is_done = 1;
        } else if (plain) {
             if (strcmp(cmd + i, "|") == 0) {
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
                fputs("Parse Error: Command with no arguments\n", stderr);
                //free here
                free_command(head);
                return NULL;
            }

            c->argc = argc;
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

