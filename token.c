#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "token.h"
#include "alias.h"
#include "error.h"

const struct ltoken_t null_ltok = {0, MODE_HEAD_FLAG, 0};

const char n_stok = 10;
const char *spec_tok[] = {"&&", "&", "|", "<", ">", "2>", "(", ")", ";", "~"};

void free_tok(struct token_t *tok)
{
    free(tok->val);
    tok->val = NULL;
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

void update_mode(struct ltoken_t *ltok, const struct token_t tok) {
    switch (tok.type) {
        case TOK_SPLIT:
            if ((ltok->mode & MODE_HEAD_FLAG) && is_string_token(*get_last_tok(ltok))) //currently head and have typed
                ltok->mode &= ~MODE_HEAD_FLAG;
            break;
        case TOK_SQU:
            ltok->mode ^= MODE_SQUOTE_FLAG;
            break;
        case TOK_DQU:
            ltok->mode ^= MODE_DQUOTE_FLAG;
            break;
        default:
            if (is_break_token(tok))
                ltok->mode |= MODE_HEAD_FLAG;
    }
}

void remove_last_token(struct ltoken_t *ltok)
{
    if (!ltok->n)
        return;

    free_tok(&ltok->tok[--ltok->n]);
}

struct token_t *get_last_tok(struct ltoken_t *ltok)
{
    static struct token_t tok_null;

    if (!ltok->n) {
        tok_null.type = TOK_NULL;
        tok_null.val = NULL;
        return &tok_null;
    }

    return &ltok->tok[ltok->n - 1];
}

void str_merge(char **a, char *b) {
    int lena = strlen(*a);
    int len = lena + strlen(b);
    *a = realloc(*a, len + 1);

    strcpy(*a + lena, b);
    free(b);
}

void add_token(struct ltoken_t *ltok, struct token_t tok)
{
    update_mode(ltok, tok);

    if (is_quote_token(tok))
        tok = get_tok_arg(TOK_NARG, calloc(1, 1));

    struct token_t *last_tok = get_last_tok(ltok);

    if (last_tok->type == TOK_END) {
        remove_last_token(ltok);
        last_tok = get_last_tok(ltok);
    }

    if (is_arg_token(tok) && is_arg_token(*last_tok)) {
        if (tok.type == TOK_NARG)
            last_tok->type = TOK_NARG;

        str_merge(&last_tok->val, tok.val);
    } else {
        if (tok.type == TOK_SPLIT && !is_arg_token(*last_tok))
            return; //only add tok_split  arg token

        ++ltok->n;
        ltok->tok = realloc(ltok->tok, ltok->n * sizeof(struct token_t));
        ltok->tok[ltok->n-1] = tok;
    }
}

static inline struct token_t get_tok_char(int type, char t)
{
    assert(type == TOK_ARG || type == TOK_NARG);

    char *s = calloc(2, sizeof(t));
    s[0] = t;
    s[1] = '\0';

    struct token_t tok = {type, s};
    return tok;
}

static struct token_t get_dollar_token(const char ** s) {
    int len = 0;
    while (isalnum((*s)[len]))
        ++len;

    char *arg = malloc(len + 1);
    memcpy(arg, *s, len);
    arg[len] = '\0';

    *s += len;
    return get_tok_arg(TOK_DOLLAR, arg);
}

static struct token_t next_token(const char **s, int mode)
{
    if (!*s || !**s)
        return get_tok(TOK_END);

    char t = **s;
    ++(*s);

    if (isspace(t) && is_mode_normal(mode)) {
        while (isspace(**s))
            ++*s;
        return get_tok(TOK_SPLIT);
    }

    if (!(mode & MODE_SQUOTE_FLAG) && t == '$')
        return get_dollar_token(s);

    if (t == '"' && (is_mode_normal(mode) || (mode & MODE_DQUOTE_FLAG)) )
        return get_tok(TOK_DQU);

    if (t == '\'' && (is_mode_normal(mode) || (mode & MODE_SQUOTE_FLAG)) )
        return get_tok(TOK_SQU);

    if (mode & MODE_SQUOTE_FLAG)
        return get_tok_char(TOK_NARG, t); //get the current
    
    if (mode & MODE_DQUOTE_FLAG)
        return get_tok_char(TOK_ARG, t);

    --(*s);
    //check for special tok
    for (int k = 0; k < n_stok; ++k) {
        int len = compare_prefix(*s, spec_tok[k]);
        if (len > 0) {
            *s += len;
            return get_tok(k);
        }
    }

    ++(*s);
    return get_tok_char(TOK_ARG, t);
}

//only tokenize when have spaceing
void plain_tokenize(const char *cmd, struct ltoken_t *ltok)
{
    while (*cmd) {
        if (isspace(*cmd)) {
            add_token(ltok, get_tok(TOK_SPLIT));
            do {
            ++cmd;
            } while (isspace(*cmd));
        } else {
            add_token(ltok, get_tok_char(TOK_NARG, *cmd));
        }
    }
}

//convert to token and append to ltok
void tokenize(const char *cmd, struct ltoken_t *ltok)
{
    const char *s = cmd;
    struct token_t btok;

    while(1) {
        const char *t = s;
        btok = next_token(&s, ltok->mode);

        if (btok.type == TOK_TILDE) {
            if (!is_string_token(*get_last_tok(ltok)))
                btok = get_tok_arg(TOK_DOLLAR, strdup("HOME"));
            else
                btok = get_tok_char(TOK_ARG, '~');
        }

        struct token_t *last_tok = get_last_tok(ltok);

        if (!is_string_token(btok) && !is_quote_token(btok) && 
            (ltok->mode & MODE_HEAD_FLAG) && last_tok->type == TOK_ARG) {

            int al_id = find_alias(last_tok->val);
            if (al_id >= 0) {
                //remove last token :))
                free_tok(&btok);
                remove_last_token(ltok);
                set_alias_state(al_id, 0);
                tokenize(get_alias_cmd(al_id), ltok);
                set_alias_state(al_id, 1);

                s = t; //undo S
                continue;
            }
        }

        add_token(ltok, btok);

        if (btok.type == TOK_END)
            break;
    }
}

void token_expand(struct ltoken_t *old_ltok, struct ltoken_t *ltok)
{
    int same_ltok = old_ltok == ltok;
    struct ltoken_t temp_ltok = null_ltok;
    if (same_ltok)
        ltok = &temp_ltok;
    
    int i;
    for (i = 0; i < old_ltok->n; ++i) {
        struct token_t *tok = &old_ltok->tok[i];
        if (tok->type == TOK_DOLLAR) {
            char *env = getenv(tok->val);
            if (!env)
                env = "";

            add_token(ltok, get_tok_arg(TOK_NARG, strdup(env)));
            free_tok(tok);
        } else {
            add_token(ltok, *tok);
            tok->val = NULL;
        }
    }

    free_ltok(old_ltok);

    if (same_ltok)
        *old_ltok = *ltok;
}