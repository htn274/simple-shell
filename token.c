#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "token.h"
#include "alias.h"
#include "error.h"

const struct ltoken_t null_ltok = {0,0};
const struct token_t null_tok = {TOK_ARG, NULL};

const char n_stok = 9;
const char *spec_tok[] = {"&&", "&", "|", "<", ">", "2>", "(", ")", ";"};

int get_tok_len(int type)
{
    if (type == TOK_ARG)
        return -1;

    if (type == TOK_END)
        return 0;

    if (type == TOK_SPACE || type == TOK_DOLLAR || type == TOK_TILDE)
        return 1;
    
    return strlen(spec_tok[type]);
}

void free_tok(struct token_t *tok)
{
    free(tok->val);
}

void free_ltok(struct ltoken_t *ltok)
{
    int i;
    for (i = 0; i < ltok->n; ++i)
        free_tok(ltok->tok + i);

    free(ltok->tok);
}

struct token_t clone_tok(const struct token_t tok)
{
    struct token_t ctok;
    ctok.type = tok.type;

    if (tok.val)
        ctok.val = strdup(tok.val);
    else
        ctok.val = NULL;
    
    return ctok;
}

void add_token(struct ltoken_t *ltok, const struct token_t tok)
{
    if (tok.type == TOK_SPACE || tok.type == TOK_TILDE || tok.type == TOK_DOLLAR)
        return;

    ++ltok->n;
    ltok->tok = realloc(ltok->tok, ltok->n * sizeof(struct token_t));
    ltok->tok[ltok->n-1] = tok;
}

//if t is prefix of s
static inline int compare_prefix(const char *s, const char *t) {
    int i = 0;
    while (s[i] && t[i] && s[i] == t[i])
        ++i;

    if (!t[i])
        return i;
    
    return 0;
}

static inline int next_token(char const *s)
{
    if (isspace(*s))
        return TOK_SPACE;

    if (*s == '~')
        return TOK_TILDE;

    if (*s == '$')
        return TOK_DOLLAR;

    //checkfor special tok
    for (int k = 0; k < n_stok; ++k) {
        int len = compare_prefix(s, spec_tok[k]);
        if (len) {
            s += len;
            return k;
        }
    }

    return TOK_ARG;
}

//convert to token
int tokenize(const char *cmd, struct ltoken_t *ltok)
{
    struct subs {
        const char *str;
        int alias;
    };

    //stack
    int cap = 0; //capacity of stack
    int top = 0; //top of stack
    struct subs *stack = NULL;

    #define push(str, id) {if (top >= cap) {cap = top + 1; stack = realloc(stack, cap * sizeof(struct subs));}; stack[top++]= (struct subs){(str), (id)};}

    push(" ", -2);
    push(cmd, -1);

    int val_cap = 100;
    char *val = malloc(val_cap * sizeof(char)); //need to change after use stack

    #define add_val(c) {if (j >= val_cap) {val_cap = j + 1; val = realloc(val, val_cap * sizeof(char));}; val[j++] = (c);}

    *ltok = null_ltok;

    char delim = ' ';

    int plain = 1;
    int j = 0;

    int aliasible = 1;

    while (top) {
        int cur_top = top;

        const char *s = stack[top - 1].str;

        if (!*s) {
            set_alias_state(stack[top - 1].alias, 1);
            aliasible = 1;
            --top;
            continue;
        }

        int btok = TOK_END; //breaking token

        while(*s) {
            if (aliasible && delim != '\'' && *s == '$') {
                btok = TOK_DOLLAR; //Dollar sign
                break;
            }

            if (delim != ' ') {
                if (*s == delim)
                    delim = ' ';
                else
                    add_val(*s);
            } else {
                btok = next_token(s);
                if (btok != TOK_ARG)
                    break;

                if (*s == '"' || *s == '\'') {
                    delim = *s;
                    plain = 0;
                } else
                    add_val(*s);
            }
            ++s;
        }

        int alias_id;
        int stop_tok = is_stop_tok(btok);

        add_val('\0');
        if (!aliasible || !val[0] || !plain || !stop_tok ||
            (ltok->n && !is_stop_tok(ltok->tok[ltok->n-1].type)) || 
            (alias_id = find_alias(val)) < 0
        ) {
            if (stop_tok) {
                if (val[0])
                    add_token(ltok, get_arg(strdup(val)));
                add_token(ltok, get_tok(btok));
                s += get_tok_len(btok);

                plain = 1;
                j = 0;
            } else {
                --j;
            }

        } else {
            set_alias_state(alias_id, 0);
            push(get_alias_cmd(alias_id), alias_id);
            j = 0;
        }

        if (btok == TOK_DOLLAR) {
            ++s; //skip the dollar
            const char *s0 = s; 
            int n = 0;
            while (*s && isalnum(*s)) {
                ++n;
                ++s;
            }
            
            if (s0 == s) {
                add_val('$');
            } else {
                char *var = malloc((n + 1) * sizeof(char));
                memcpy(var, s0, n * sizeof(char));
                var[n] = '\0';
                char *var_s = getenv(var);
                free(var);
                
                if (var_s) {
                    push(var_s, -1);
                    plain = 0;
                    aliasible = 0;
                }
            }
        } else if (btok == TOK_TILDE) {
            ++s; //skip the tilde

            if (plain && aliasible && j == 0) {
                push(getenv("HOME"), -1);
                plain = 0;
                aliasible = 0;
            } else {
                add_val('~');
            }
        }

        stack[cur_top - 1].str = s;
    }

    if (delim != ' ') {
        error_str = "Parse Error: Not closing \" or '\n";
        goto token_error;
    } 

    free(stack);
    free(val);
    return 0;
token_error:
    free_ltok(ltok);
    free(stack);
    free(val);
    return -1;
}
