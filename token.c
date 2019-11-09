#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "token.h"
#include "alias.h"
#include "error.h"

const struct ltoken_t null_ltok = {0, 0, 0};

const struct token_map_t spec_tok[] = {
    {"&&", get_tok(TOK_AND)},
    {"&", get_tok(TOK_ASYNC)},
    {"|", get_tok(TOK_PIPE)},
    {"<", get_tok(TOK_RDR_IN)},
    {">", get_tok(TOK_RDR_OUT)},
    {"2>", get_tok(TOK_RDR_ERR)},
    {"(", get_tok(TOK_OBRACK)},
    {")", get_tok(TOK_CBRACK)},
    {";", get_tok(TOK_SEMICOL)},
    {"\n", get_tok(TOK_SEMICOL)},
    {"*", get_tok(TOK_STAR)},
    {"?", get_tok(TOK_QMARK)}
};

const char n_stok = sizeof(spec_tok)/sizeof(spec_tok[0]);

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

void update_mode(struct ltoken_t *ltok, const struct token_t tok) {
    switch (tok.type) {
        case TOK_SQU:
            ltok->mode ^= MODE_SQUOTE_FLAG;
            break;
        case TOK_DQU:
            ltok->mode ^= MODE_DQUOTE_FLAG;
            break;
    }
}

void remove_last_token(struct ltoken_t *ltok)
{
    if (!ltok->n)
        return;

    free_tok(&ltok->tok[--ltok->n]);
}


struct token_t *get_tok_offset(struct ltoken_t *ltok, int offset)
{
    static struct token_t tok_null;

    if (offset > ltok->n) {
        tok_null.type = TOK_NULL;
        tok_null.len = 0;
        tok_null.val = NULL;
        return &tok_null;
    }

    return &ltok->tok[ltok->n - offset];
}

struct token_t *get_last_tok(struct ltoken_t *ltok)
{
    return get_tok_offset(ltok, 1);
}

int is_last_tok_aliasible(struct ltoken_t *ltok) {
    if (get_tok_offset(ltok, 1)->type != TOK_ARG)
        return 0;

    if (is_string_token(*get_tok_offset(ltok, 2)))
        return 0;

    if (get_tok_offset(ltok, 2)->type == TOK_SPLIT)
        return 0; //not really but most of the case unless redirect first

    return 1;
}

void str_merge(struct token_t *a, struct token_t *b) {
    int len = a->len + b->len;
    a->val = realloc(a->val, len + 1);

    memcpy(a->val + a->len, b->val, b->len + 1);
    a->len = len;

    free_tok(b);
}

void add_token(struct ltoken_t *ltok, struct token_t tok)
{
    if(tok.type == TOK_NULL)
        return;

    update_mode(ltok, tok);

    if (is_quote_token(tok))
        tok = get_tok_arg(TOK_NARG, 0, calloc(1, 1));

    struct token_t *last_tok = get_last_tok(ltok);

    if (last_tok->type == TOK_END) {
        remove_last_token(ltok);
        last_tok = get_last_tok(ltok);
    }

    if (is_arg_token(tok) && last_tok->type == tok.type) {
        str_merge(last_tok, &tok);
    } else {
        if (tok.type == TOK_SPLIT && !is_string_token(*get_last_tok(ltok)))
            return;
    
        ++ltok->n;
        ltok->tok = realloc(ltok->tok, ltok->n * sizeof(struct token_t));
        ltok->tok[ltok->n-1] = tok;
    }
}

static inline struct token_t get_tok_char(int type, char t)
{
    assert(type == TOK_ARG || type == TOK_NARG);

    if (t == '\0')
        return get_tok(TOK_NULL);

    char *s = calloc(2, sizeof(t));
    s[0] = t;
    s[1] = '\0';

    return get_tok_arg(type, 1, s);
}

int is_var_name(char c) {
    return isalnum(c) || c == '_';
}

static struct token_t get_dollar_token(const char ** s) {
    int len = 0;
    while (is_var_name((*s)[len]))
        ++len;

    char *arg = malloc(len + 1);
    memcpy(arg, *s, len);
    arg[len] = '\0';

    *s += len;
    return get_tok_arg(TOK_DOLLAR, len, arg);
}

static struct token_t get_escape_token(const char ** s) {
    static const char from[] = {'\\', '0', 'b', 'n', 'r', 't'};
    static const char to[] = {'\\', '\0', '\b', '\n', '\r', '\t'};
    int i;
    for (i = 0; i < sizeof(from); ++i)
        if (from[i] == **s) {
            ++*s;
            return get_tok_char(TOK_NARG, to[i]);
        }

    return get_tok_char(TOK_ARG, '\\');
}

static struct token_t next_token(const char **s, struct ltoken_t * ltok)
{
    if (!*s || !**s)
        return get_tok(TOK_END);

    char t = **s;
    ++(*s);

    int mode = ltok->mode;

    if (isspace(t) && is_mode_normal(mode)) {
        while (isspace(**s))
            ++*s;

        return get_tok(TOK_SPLIT);
    }

    if (!(mode & MODE_SQUOTE_FLAG)) {
        if (t == '$')
            return get_dollar_token(s);
        if (t == '\\') {
            if (mode & MODE_DQUOTE_FLAG)
                return get_escape_token(s);
            else if (**s)
                return get_tok_char(TOK_NARG, *((*s)++));
            else
                return get_tok(TOK_NULL);
        }
    }

    if (t == '"' && (is_mode_normal(mode) || (mode & MODE_DQUOTE_FLAG)) )
        return get_tok(TOK_DQU);

    if (t == '\'' && (is_mode_normal(mode) || (mode & MODE_SQUOTE_FLAG)) )
        return get_tok(TOK_SQU);

    if (mode & MODE_SQUOTE_FLAG)
        return get_tok_char(TOK_NARG, t); //get the current
    
    if (mode & MODE_DQUOTE_FLAG)
        return get_tok_char(TOK_ARG, t);

    if (t == '~') {
        if (is_string_token(*get_last_tok(ltok)))
            return get_tok_char(TOK_ARG, '~');
        else
            return get_tok(TOK_TILDE);
    }

    --(*s);
    //check for special tok
    for (int k = 0; k < n_stok; ++k) {
        int len = compare_prefix(*s, spec_tok[k].str);
        if (len > 0) {
            *s += len;
            return spec_tok[k].tok;
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
        btok = next_token(&s, ltok);

        if (!is_string_token(btok) && !is_quote_token(btok) && 
            is_last_tok_aliasible(ltok)) {

            struct token_t *last_tok = get_last_tok(ltok);

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

void add_env(struct ltoken_t *ltok, const char *var) {
    char *env = getenv(var);
    if (!env)
        env = "";

    add_token(ltok, get_tok_arg(TOK_NARG, strlen(env), strdup(env)));
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
            add_env(ltok, tok->val);
            free_tok(tok);
        } else if (tok->type == TOK_TILDE) {
            add_env(ltok, "HOME");
        } else {
            if (is_arg_token(*tok))
                tok->type = TOK_NARG;
            add_token(ltok, *tok);
            tok->val = NULL;
        }
    }

    free_ltok(old_ltok);

    if (same_ltok)
        *old_ltok = *ltok;
}