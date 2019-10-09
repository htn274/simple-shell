#ifndef _BUILTIN_
#define _BUILTIN_

struct builtin_t {
    char *name;
    int (*fun)(char **args);
};

extern int bin_cnt;
extern struct builtin_t bin_fun[];

int exec_builtin(char **args, int *res);

#endif