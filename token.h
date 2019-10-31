#ifndef _TOKEN_
#define _TOKEN_

#ifndef NULL
#define NULL (void *)0
#endif 


struct token_t {
    int type; //-1 if not special token
    char *val;
};

struct ltoken_t {
    int n;
    int mode; //current mode at the end of the list
    struct token_t *tok;
};

extern const struct ltoken_t null_ltok;
extern const char n_stok;
extern const char *spec_tok[];


#define TOK_NULL -4 //token of null
#define TOK_END -3 //only end stack
#define TOK_NARG -2 //args that can't alias
#define TOK_ARG -1
#define TOK_AND 0
#define TOK_ASYNC 1
#define TOK_PIPE 2
#define TOK_RDR_IN 3
#define TOK_RDR_OUT 4
#define TOK_RDR_ERR 5
#define TOK_OBRACK 6 //open bracket
#define TOK_CBRACK 7 //close bracket
#define TOK_SEMICOL 8
#define TOK_TILDE 9

#define TOK_DOLLAR 50

#define TOK_SPLIT 100
#define TOK_SQU 101 //single quote
#define TOK_DQU 102 //double quote

#define MODE_HEAD_FLAG 1  //mode normall currently enter the head arg of the command
#define MODE_SQUOTE_FLAG 2 //mode single quote
#define MODE_DQUOTE_FLAG 4 //mode double quote

static inline int is_mode_normal(int mode)
{
    return !(mode & (MODE_DQUOTE_FLAG | MODE_SQUOTE_FLAG));
}

static inline struct token_t get_tok_arg(int type, char *s)
{
    struct token_t tok = {type, s};
    return tok;
}


static inline struct token_t get_tok(int type)
{
    return get_tok_arg(type, NULL);
}


static inline int is_arg_token(const struct token_t tok) {
    return tok.type == TOK_ARG || tok.type == TOK_NARG;
}


static inline int is_string_token(const struct token_t tok) {
    return is_arg_token(tok) || tok.type == TOK_DOLLAR;
}

static inline int is_quote_token(const struct token_t tok) {
    return tok.type == TOK_SQU || tok.type == TOK_DQU;
}

//token that break the command and start another command
static inline int is_break_token(const struct token_t tok) {
    return tok.type == TOK_PIPE || tok.type == TOK_SEMICOL || tok.type == TOK_AND;
}

static inline struct token_t *get_last_tok(struct ltoken_t *ltok) {
    static struct token_t tok_null;

    if (!ltok->n) {
        tok_null.type = TOK_NULL;
        tok_null.val = NULL;
        return &tok_null;
    }

    return &ltok->tok[ltok->n - 1];
}

//check if t is the prefix of s. return t length if true (can be zero) -1 if false
static inline int compare_prefix(const char *s, const char *t) {
    int i = 0;
    while (s[i] && t[i] && s[i] == t[i])
        ++i;

    if (!t[i])
        return i;
    
    return -1;
}

struct token_t clone_tok(const struct token_t tok);
void free_tok(struct token_t *tok);
void free_ltok(struct ltoken_t *ltok);

//expand DOLLAR token (not support wildcards yet) this will destroy old_ltok
//also need to handle two ltok is the same
void token_expand(struct ltoken_t *old_ltok, struct ltoken_t *ltok);

void add_token(struct ltoken_t *ltok, struct token_t tok);
void tokenize(const char *cmd, struct ltoken_t *ltok);
void plain_tokenize(const char *cmd, struct ltoken_t *ltok);

#endif