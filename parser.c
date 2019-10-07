#include "command.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static const char n_stok = 8;
static const char *spec_tok[] = {"&&", "&", "|", "<", ">", "(", ")", ";"};

#define TOK_ARG -1
#define TOK_AND 0
#define TOK_ASYNC 1
#define TOK_PIPE 2
#define TOK_RDR_IN 3
#define TOK_RDR_OUT 4
//#define TOK_OBRACK 5 //open bracket
//#define TOK_CBRRACK 6 //close bracket
#define TOK_SEMICOL 7

char *error_str = "no error\n";

struct token_t {
    int type; //-1 if not special token
    char *val;
};

struct ltoken_t {
    int n;
    struct token_t *tok;
};

const struct ltoken_t null_ltok = {0,0};

void free_command(struct command *c) {
    int i;
    for (i = 0; i < c->argc; ++i)
        free(c->args[i]);

    free(c->args);

    free(c->filename[0]);
    free(c->filename[1]);
}

void free_lcommand(struct lcommand *c)
{
    int i;
    for (i = 0; i < c->n; ++i)
        free_command(c->c + i);
    
    free(c->c);
}

void free_ltok(struct ltoken_t *ltok)
{
    int i;
    for (i = 0; i < ltok->n; ++i) {
        if (ltok->tok[i].type == TOK_ARG)
            free(ltok->tok[i].val);
    }

    free(ltok->tok);
}

static inline int skip_space(const char *s, int i)
{
    while (s[i] && isspace(s[i]))
        ++i;

    return i;
}

void add_token(struct ltoken_t *ltok, const struct token_t tok)
{
    ++ltok->n;
    ltok->tok = realloc(ltok->tok, ltok->n * sizeof(struct token_t));
    ltok->tok[ltok->n-1] = tok;
}

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

static inline int compare_tok(const char *s, int tok) {
    int i = 0;
    const char *t = spec_tok[tok];
    while (s[i] && t[i] && s[i] == t[i])
        ++i;

    if (!t[i])
        return i;
    
    return 0;
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
                    int len = compare_tok(s + i, k);
                    if (len) {
                        i += len;
                        special = k;
                        break;
                    }
                }

                if (special >= 0 || !s[i] || isspace(s[i])) {
                    if (j) {
                        val[j++] = '\0';
                        add_token(ltok, get_arg(strdup(val)));
                    }

                    if (special >= 0)
                        add_token(ltok, get_tok(special));

                    break;
                }

                if (s[i] == '"' || s[i] == '\'')
                    delim = s[i];
                else
                    val[j++] = s[i];
            }
            ++i;
        };
    }

    if (ltok->n && ltok->tok[ltok->n-1].type != TOK_SEMICOL)
        add_token(ltok, get_tok(TOK_SEMICOL));

    free(val);
    return 0;
token_error:
    free_ltok(ltok);
    free(val);
    return -1;
}

int parse_command(const char *s, struct lcommand *cmd) {
    struct ltoken_t ltok = null_ltok;
    if (tokenize(s, &ltok))
        return -1;
    

    *cmd = null_lcmd;
    struct command c = null_cmd;

    int i;
    for (i = 0; i < ltok.n; ++i) {
        int is_done = 0;

        switch (ltok.tok[i].type)
        {
        case TOK_ASYNC:
            c.async = 1;
            break;
        case TOK_PIPE:
            c.pipe = 1;
        case TOK_AND: case TOK_SEMICOL:
            is_done = 1;
            break;
        case TOK_RDR_IN: case TOK_RDR_OUT:
            if (i + 1 >= ltok.n || ltok.tok[i+1].type != TOK_ARG) {
                error_str = "Parser Error: Missing filename after redirection\n";
                goto parse_error;
            }

            int type = ltok.tok[i].type == TOK_RDR_OUT;

            if (!c.filename[type]) {
                ++i;
                c.filename[type] = ltok.tok[i].val;
                ltok.tok[i].val = NULL;
                continue;
            } else {
                error_str = "Parser Error: Cannot redirect from/to 2 files\n";
                goto parse_error;
            }
        case TOK_ARG:
            ++c.argc;
            c.args = realloc(c.args, sizeof(char*) * (c.argc + 1));
            c.args[c.argc - 1] = ltok.tok[i].val;
            ltok.tok[i].val = NULL;
            break;
        default:
            break; 
        }

        if (is_done) {
            if (!c.args) {
                error_str = "Parse Error: Command with no arguments\n";
                goto parse_error;
            }

            c.args[c.argc] = NULL;
            //add to head
            ++cmd->n;
            cmd->c = realloc(cmd->c, sizeof(struct command) * cmd->n);
            cmd->c[cmd->n-1] = c;

            c = null_cmd;
        }
    }
    
    free_ltok(&ltok);
    return 0;

parse_error:
    free_ltok(&ltok);
    free_command(&c);
    free_lcommand(cmd);
    return -1;
}

