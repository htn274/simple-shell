
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "builtin.h"
#include "error.h"
#include "alias.h"

int exec_builtin(char **args, int *res)
{
    for (int i = 0; i < bin_cnt; ++i)
        if (strcmp(bin_fun[i].name, args[0]) == 0) {
            int r = bin_fun[i].fun(args);
            if (res)
                *res = r;
            return 1;
        }
    return 0;
}

int shell_exit(char **args) {
    if (args[1] && args[2]) {
        fputs("exit error: Too many arguments\n", stderr);
        return -1;
    }
    
    if (args[1])
        exit(atoi(args[1]));
    else
        exit(0);
}

int cd(char **args) {
    int res = 0;
    if (!args[1])
        res = chdir(getenv("HOME"));
    else if (args[2])
        fputs("cd error: Too many arguments\n", stderr);
    else  {
        if (args[1][0] == '~') {
            res = chdir(getenv("HOME"));
            args[1][0] = '.';
        }
        
        if (res >= 0)
            res = chdir(args[1]);
    }
    if (res < 0)
        perror("cd error");

    return res;
}

int set(char **args) {
    if (!args[1] || !args[2]){
        fputs("set error: Too few arguments\n", stderr);
        return -1;
    }
    if (args[3]) {
        fputs("set error: Too many arguments\n", stderr);
        return -1;
    }

    if (setenv(args[1], args[2], 1) < 0) {
        perror("set error");
        return -1;
    }
    return 0;
}

int unset(char **args) {
    if (!args[1]){
        fputs("unset error: Too few arguments\n", stderr);
        return -1;
    }
    if (args[2]) {
        fputs("unset error: Too many arguments\n", stderr);
        return -1;
    }

    if (unsetenv(args[1]) < 0) {
        perror("unset error");
        return -1;
    }

    return 0;
}

int alias(char **args) {
    if (!args[1]){
        fputs("alias error: Too few arguments\n", stderr);
        return -1;
    } else if (!args[2]) {
        const char *cmd = find_alias(args[1]);
        if (cmd)
            printf("alias %s='%s'\n", args[1], cmd);
        else
            printf("alias %s is not exist\n", args[1]);
        return 0;
    } else if(args[3]) {
        fputs("alias error: Too many arguments\n", stderr);
        return -1;
    } else {
        if (set_alias(args[1], args[2]) < 0) {
            fputs(error_str, stderr);
            return -1;
        }
        return 0;
    }
}

int unalias(char **args) {
    if (!args[1]){
        fputs("alias error: Too few arguments\n", stderr);
        return -1;
    } else if (args[2]) {
        fputs("alias error: Too many arguments\n", stderr);
        return -1;
    } else {    
        if (unset_alias(args[1]) < 0) {
            printf("alias %s is not exist\n", args[1]);
            return -1;
        }
        return 0;
    }
}


int bin_cnt = 6;
struct builtin_t bin_fun[] = {{"exit", shell_exit},
                              {"cd", cd},
                              {"set", set},
                              {"unset", unset},
                              {"alias", alias},
                              {"unalias", unalias}
                              };
