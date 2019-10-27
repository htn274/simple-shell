#ifndef _BUILTIN_
#define _BUILTIN_

struct builtin_t {
    char *name;
    int (*fun)(char **args);
};

extern int bin_cnt;
extern struct builtin_t bin_fun[];

typedef int fd_list[3];

int exec_builtin(char **args, int *res, fd_list fd);

#endif