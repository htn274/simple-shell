#ifndef _CMD_
#define _CMD_

struct command {
    int argc;
    char **args;
    int async;

    char *filename[2];
    int pipe; //true mean pipe, when pipe to next command

    struct command *next;
};

void free_command(struct command *c);
int exec_command(struct command *cmd);
struct command *parse_command(char *cmd);

#endif