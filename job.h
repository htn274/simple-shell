#ifndef _JOB_
#define _JOB_

#include <sys/types.h>

#ifndef NULL
#define NULL (void *)0
#endif 

struct job_t {
    pid_t pid;
    char *cmd;
    int running;
}; //job table

static const struct job_t null_job = {(pid_t)-1,NULL, 1};

struct ljob_t {
    int cap;
    struct job_t *job;
};

static const struct ljob_t null_ljob = {0,NULL};

int add_job(struct ljob_t *job_tab);
int get_empty_job(struct ljob_t *job_tab);
void free_ljob(struct ljob_t *job_tab);
void sigchild_handler(int sig);

#endif