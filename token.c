#include "token.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

const struct ltoken_t null_ltok = {0,0};

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

void add_single_token(struct ltoken_t *ltok, const struct token_t tok)
{
    ++ltok->n;
    ltok->tok = realloc(ltok->tok, ltok->n * sizeof(struct token_t));
    ltok->tok[ltok->n-1] = tok;

    if (tok.type == TOK_ARG_ALIAS)
        ltok->tok[ltok->n-1].type = TOK_ARG;
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


static inline int skip_space(const char *s, int i)
{
    while (s[i] && isspace(s[i]))
        ++i;

    return i;
}

//convert to token
int tokenize(const char *s, struct ltoken_t *ltok)
{
    char *val = malloc((strlen(s) + 1) * sizeof(char));

    *ltok = null_ltok;

    int i = 0;

    while (1) {
        i = skip_space(s, i);

        if (!s[i])
            break;

        int j = 0;
        char delim = ' ';

        int plain = 1;

        while(1) {
            if (delim != ' ') {
                if (!s[i]) {
                    error_str = "Parse Error: Not closing \" or '\n";
                    goto token_error;
                } else if (s[i] == delim)
                    delim = ' ';
                else
                    val[j++] = s[i];
            } else {
                int special = -1;
                //checkfor special tok
                for (int k = 0; k < n_stok; ++k) {
                    int len = compare_prefix(s + i, spec_tok[k]);
                    if (len) {
                        i += len;
                        special = k;
                        break;
                    }
                }

                if (special >= 0 || !s[i] || isspace(s[i])) {
                    if (j) {
                        val[j++] = '\0';

                        struct token_t tok;
                        if (plain && ((!ltok->n) || ltok->tok[ltok->n-1].type != -1))
                            tok = get_arg_alias(val);
                        else
                            tok = get_arg(val);

                        add_token(ltok, tok);
                    }

                    if (special >= 0)
                        add_token(ltok, get_tok(special));

                    break;
                }

                if (s[i] == '"' || s[i] == '\'') {
                    delim = s[i];
                    plain = 0;
                } else
                    val[j++] = s[i];
            }
            ++i;
        };
    }

    free(val);
    return 0;
token_error:
    free_ltok(ltok);
    free(val);
    return -1;
}
