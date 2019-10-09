#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "token.h"
#include "alias.h"
#include "error.h"

const struct ltoken_t null_ltok = {0,0};
const struct token_t null_tok = {TOK_NULL, NULL};

const char n_stok = 9;
const char *spec_tok[] = {"&&", "&", "|", "<", ">", "2>", "(", ")", ";"};

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
    if (is_null_tok(tok))
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

static inline const char *skip_space(const char *s)
{
    while (*s && isspace(*s))
        ++s;

    return s;
}

//convert to token
int tokenize(const char *cmd, struct ltoken_t *ltok)
{
    struct subs {
        const char *str;
        int alias;

        struct token_t tok_after;
    };

    //stack
    int cap = 0; //capacity of stack
    int top = 0; //top of stack
    struct subs *stack = NULL;

    #define push(str, id, tok) {if (top >= cap) {cap = top + 1; stack = realloc(stack, cap * sizeof(struct subs));}; stack[top++]= (struct subs){(str), (id), (tok)};}

    push(cmd, -1, null_tok);

    int val_cap = 100;
    char *val = malloc(val_cap * sizeof(char)); //need to change after use stack

    #define add_val(c) {if (j >= val_cap) {val_cap = j + 1; val = realloc(val, val_cap * sizeof(char));}; val[j++] = (c);}

    *ltok = null_ltok;

    char delim = ' ';

    int plain = 1;

    while (top) {
        int cur_top = top;
        const char *s = skip_space(stack[top - 1].str);

        if (!*s) {
            add_token(ltok, stack[top - 1].tok_after);
            set_alias_state(stack[top - 1].alias, 1);
            --top;
            continue;
        }

        int j = 0;

        while(1) {
            if (delim != ' ') {
                if (!*s)
                    break;
                else if (*s == delim)
                    delim = ' ';
                else
                    add_val(*s);
            } else {
                int special = -1;
                //checkfor special tok
                for (int k = 0; k < n_stok; ++k) {
                    int len = compare_prefix(s, spec_tok[k]);
                    if (len) {
                        s += len;
                        special = k;
                        break;
                    }
                }

                if (special >= 0 || !*s || isspace(*s)) {
                    add_val('\0');
                    int alias_id;
                    if (!val[0] || 
                        !plain || 
                        (ltok->n && ltok->tok[ltok->n-1].type < 0) || 
                        (alias_id = find_alias(val)) < 0
                    ) {
                        if (!plain || val[0])
                            add_token(ltok, get_arg(strdup(val)));
    
                        if (special >= 0)
                            add_token(ltok, get_tok(special));
                    } else {
                        set_alias_state(alias_id, 0);
                        push(get_alias(alias_id), 
                                alias_id, 
                                special >= 0 ? get_tok(special) : null_tok 
                        );
                    }

                    plain = 1;
                    break;
                }

                if (*s == '"' || *s == '\'') {
                    delim = *s;
                    plain = 0;
                } else
                    add_val(*s);
            }
            ++s;
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
