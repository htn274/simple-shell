#ifndef _CMD_
#define _CMD_

#ifndef NULL
#define NULL (void *)0
#endif 

struct command {
    int argc;
    char **args;
    int async;

    char *filename[2];
    int pipe; //true mean pipe, when pipe to next command
};

static const struct command null_cmd = {0, NULL, 0, {NULL, NULL}, 0};

struct lcommand {
    int n;
    struct command *c;
    
    char *arg;
};

static const struct lcommand null_lcmd = {0, NULL};


void free_command(struct lcommand c);
int exec_command(struct lcommand cmd);
struct lcommand parse_command(const char *cmd);

#endif