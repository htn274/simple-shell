#include "command.h"
#include <string.h>
#include <stdlib.h>
#include "token.h"
#include "error.h"

void free_command(struct command_t *c) {
    int i;
    for (i = 0; i < c->argc; ++i)
        free(c->args[i]);

    free(c->args);

    free(c->filename[0]);
    free(c->filename[1]);
    free(c->filename[2]);

    *c = null_cmd;
}

void free_lcommand(struct lcommand_t *c)
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
        case TOK_RDR_ERR:
            type = 2;
            break;
    }
    return type;
}

int str_cat(char **str, int n, char *arg) {
    int l = strlen(arg);
    *str = realloc(*str, (n + l + 2) * sizeof(char));
    strcpy(*str + n, arg);
    (*str)[n + l] = ' ';
    return l + 1;
}


char *cmd_to_string(const struct command_t *cmd) {
    int i;

    int n = 0;
    char *str = malloc(1);
    for (i = 0; i < cmd->argc; ++i) {
        char *arg = cmd->args[i];
        n += str_cat(&str, n, arg);
    }

    if (cmd->filename[0]) {
        n += str_cat(&str, n, "<");
        n += str_cat(&str, n, cmd->filename[0]);
    }

    if (cmd->filename[1]) {
        n += str_cat(&str, n, ">");
        n += str_cat(&str, n, cmd->filename[1]);
    }

    if (cmd->filename[2]) {
        n += str_cat(&str, n, "2>");
        n += str_cat(&str, n, cmd->filename[2]);
    }

    if (cmd->async)
        n += str_cat(&str, n, "&");

    if (n)
        str[n-1] = ';';
    str[n] = '\0';
    return str;
}

int parse_command(const char *s, struct lcommand_t *cmd) {
    struct ltoken_t ltok = null_ltok;
    *cmd = null_lcmd;
    struct command_t c = null_cmd;

    tokenize(s, &ltok);
    if (ltok.mode & MODE_SQUOTE_FLAG) {
        error_str = "Parser error: Not closing the quote '\n";
        goto parse_error;
    }

    if (ltok.mode & MODE_DQUOTE_FLAG) {
        error_str = "Parser error: Not closing the quote \"\n";
        goto parse_error;
    }

    token_expand(&ltok, &ltok);
    //add ending
    if (ltok.n && ltok.tok[ltok.n-1].type != TOK_SEMICOL)
        add_token(&ltok, get_tok(TOK_SEMICOL));

    int i;
    for (i = 0; i < ltok.n; ++i) {
        switch (ltok.tok[i].type)
        {
        case TOK_ASYNC:
            c.async = 1;
            break;
        case TOK_PIPE:
            c.pipe = 1;
            break;
        case TOK_RDR_IN: case TOK_RDR_OUT: case TOK_RDR_ERR:
            if (i + 1 >= ltok.n || !is_arg_token(ltok.tok[i+1])) {
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
        case TOK_ARG: case TOK_NARG:
            ++c.argc;
            c.args = realloc(c.args, sizeof(char*) * (c.argc + 1));
            c.args[c.argc - 1] = ltok.tok[i].val;
            ltok.tok[i].val = NULL;
            break;
        default:
            break; 
        }

        if (is_break_token(ltok.tok[i])) {
            if (!c.args) {
                error_str = "Parse Error: Command with no arguments\n";
                goto parse_error;
            }

            c.args[c.argc] = NULL;
            //add to head
            ++cmd->n;
            cmd->c = realloc(cmd->c, sizeof(struct command_t) * cmd->n);
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

