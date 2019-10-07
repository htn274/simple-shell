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
};

static const struct lcommand null_lcmd = {0, NULL};

extern char *error_str;

void free_command(struct command *c);
void free_lcommand(struct lcommand *c);

int exec_lcommand(struct lcommand cmd);
int parse_command(const char *s, struct lcommand *cmd);

#endif