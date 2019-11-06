#ifndef _JOB_
#define _JOB_

#include <sys/types.h>

#ifndef NULL
#define NULL (void *)0
#endif 

#define JOB_DONE 0
#define JOB_RUNNING 1
#define JOB_STOPPED 2

struct job_t {
    pid_t pid;
    pid_t pgid;
    char *cmd;
    int running;
}; //job table

static const struct job_t null_job = {(pid_t)-1, (pid_t)-1, NULL, JOB_DONE};

struct ljob_t {
    int cap;
    struct job_t *job;
};

static const struct ljob_t null_ljob = {0, NULL};

struct job_t *get_job(struct ljob_t *job_tab, int job_id);
int add_job(struct ljob_t *job_tab);
int get_empty_job(struct ljob_t *job_tab);
void free_ljob(struct ljob_t *job_tab);
void free_job(struct job_t *job);
struct job_t job_dup(struct job_t job);

extern struct ljob_t bg_job_table;
#endif