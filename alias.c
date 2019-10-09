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

    /*if (tokenize(cmd, &dict[n_alias].cmd_tok) < 0) {
        error_str = "Alias Error: the alias command is not valid\n";
        return -1;
    }*/

    ++n_alias;
    dict[n_alias - 1].alias = strdup(alias);
    dict[n_alias - 1].cmd = strdup(cmd);
    dict[n_alias - 1].free = 1;
    return 0;
}

int unset_alias_id(int i) {
    free(dict[i].alias);
    free(dict[i].cmd);
    //free_ltok(&dict[i].cmd_tok);

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

/*void add_token(struct ltoken_t *ltok, const struct token_t tok)
{
    int k;

    if (tok.type != TOK_ARG_ALIAS || (k = find_alias(tok.val)) < 0 || !dict[k].free) {
        add_single_token(ltok, clone_tok(tok));
        return;
    }

    dict[k].free = 0;

    for (int i = 0; i < dict[k].cmd_tok.n; ++i)
        add_token(ltok, dict[k].cmd_tok.tok[i]);

    dict[k].free = 1;

}*/

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