#include "command.h"
#include <string.h>
#include <stdlib.h>
#include "token.h"
#include "error.h"

void free_command(struct command *c) {
    int i;
    for (i = 0; i < c->argc; ++i)
        free(c->args[i]);

    free(c->args);

    free(c->filename[0]);
    free(c->filename[1]);

    *c = null_cmd;
}

void free_lcommand(struct lcommand *c)
{
    int i;
    for (i = 0; i < c->n; ++i)
        free_command(c->c + i);
    
    free(c->c);

    *c = null_lcmd;
}

int get_rdr_type(int token) {
    int type = -1;
    switch(token) {
        case TOK_RDR_IN:
            type = 0;
            break;
        case TOK_RDR_OUT:
            type = 1;
            break;
        case TOK_RDR_ERR_OUT:
            type = 2;
            break;
    }
    return type;
}

int parse_command(const char *s, struct lcommand *cmd) {
    struct ltoken_t ltok = null_ltok;
    if (tokenize(s, &ltok))
        return -1;
    
    //add ending
    if (ltok.n && ltok.tok[ltok.n-1].type != TOK_SEMICOL)
        add_token(&ltok, get_tok(TOK_SEMICOL));


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
        case TOK_RDR_IN: case TOK_RDR_OUT: case TOK_RDR_ERR_OUT:
            if (i + 1 >= ltok.n || ltok.tok[i+1].type != TOK_ARG) {
                error_str = "Parser Error: Missing filename after redirection\n";
                goto parse_error;
            }

            int type = get_rdr_type(ltok.tok[i].type);

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

