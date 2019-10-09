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
    struct token_t *tok;
};

extern const struct ltoken_t null_ltok;
extern const char n_stok;
extern const char *spec_tok[];

#define TOK_ARG -1 //arg without alias haizz
#define TOK_ARG_ALIAS -2 //arg maybe with alias

#define TOK_AND 0
#define TOK_ASYNC 1
#define TOK_PIPE 2
#define TOK_RDR_IN 3
#define TOK_RDR_OUT 4
#define TOK_RDR_ERR_OUT 5
//#define TOK_OBRACK 6 //open bracket
//#define TOK_CBRRACK 7 //close bracket
#define TOK_SEMICOL 8

static inline struct token_t get_tok(int type)
{
    struct token_t tok = {type, NULL};
    return tok;
}


static inline struct token_t get_arg(char *arg)
{
    struct token_t tok = {TOK_ARG, arg};
    return tok;
}


static inline struct token_t get_arg_alias(char *arg)
{
    struct token_t tok = {TOK_ARG_ALIAS, arg};
    return tok;
}

struct token_t clone_tok(const struct token_t tok);
void free_tok(struct token_t *tok);
void free_ltok(struct ltoken_t *ltok);
void add_single_token(struct ltoken_t *ltok, const struct token_t tok);
void add_token(struct ltoken_t *ltok, const struct token_t tok); //can have token alias

int tokenize(const char *s, struct ltoken_t *ltok);

#endif