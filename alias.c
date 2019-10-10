#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "alias.h"
#include "token.h"
#include "error.h"

static int n_alias = 0;

static struct alias_t {
    char *alias;
    char *cmd;
    int free;
} *dict = NULL;

void set_alias_state(int id, int is_on)
{
    if (id < 0 || id >= n_alias)
        return;
    dict[id].free = is_on;
}

int find_alias(const char *alias) {
    int i;
    for (i = 0; i < n_alias; ++i) {
        if (dict[i].free && strcmp(dict[i].alias, alias) == 0)
            return i;
    }
    return -1;
}

const char *get_alias(int id) {
    if (id < 0 || id >= n_alias)
        return NULL;
    return dict[id].cmd;
}
int valid_name(const char *alias){
    int i;
    for(i = 0; alias[i]; ++i)
        if (!isalnum(alias[i]))
            return 0;

    return 1;
}

int add_alias(const char *alias, const char * cmd) {
    if (!valid_name(alias)) {
        error_str = "Alias Error: the alias name is not valid\n";
        return -1;
    }

    dict = realloc(dict, sizeof(struct alias_t) * (n_alias + 1));

    ++n_alias;
    dict[n_alias - 1].alias = strdup(alias);
    dict[n_alias - 1].cmd = strdup(cmd);
    dict[n_alias - 1].free = 1;
    return 0;
}

int unset_alias_id(int i) {
    free(dict[i].alias);
    free(dict[i].cmd);

    --n_alias;
    dict[i] = dict[n_alias];

    return 0;
}

int unset_alias(const char *alias)
{
    int i = find_alias(alias);
    if (i < 0)
        return -1;

    return unset_alias_id(i);
}

int set_alias(const char *alias, const char * cmd)
{
    int i = find_alias(alias);
    if (add_alias(alias, cmd) < 0)
        return -1;
    if (i >= 0)
        return unset_alias_id(i);
    return 0;
}