#ifndef _CMD_
#define _CMD_

#ifndef NULL
#define NULL (void *)0
#endif 

struct command_t {
    int argc;
    char **args;
    int async;

    char *filename[3];
    int pipe; //true mean pipe, when pipe to next command
};

static const struct command_t null_cmd = {0, NULL, 0, {NULL, NULL}, 0};


struct lcommand_t {
    int n;
    struct command_t *c;
};

static const struct lcommand_t null_lcmd = {0, NULL};

void free_command(struct command_t *c);
void free_lcommand(struct lcommand_t *c);

int exec_lcommand(struct lcommand_t cmd);
int parse_command(const char *s, struct lcommand_t *cmd);
char *cmd_to_string(const struct command_t *cmd);

int set_alias(const char *alias, const char * cmd);
int unset_alias(const char *alias);
const char *find_alias(const char *alias);

char *replace_env_var(char *s);

void sigchild_handler(int sig);

#endif