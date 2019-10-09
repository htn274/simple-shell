#include "job.h"
#include <stdlib.h>


int add_job(struct ljob_t *job_tab) {
    ++job_tab->cap;
    job_tab->job = realloc(job_tab->job , job_tab->cap * sizeof(struct job_t));
    job_tab->job[job_tab->cap - 1] = null_job;
    return job_tab->cap - 1;
}

int get_empty_job(struct ljob_t *job_tab) {
    int i;
    for (i = 0; i < job_tab->cap; ++i)
        if (!job_tab->job[i].running) {
            job_tab->job[i] = null_job;
            return i;
        }
    
    return add_job(job_tab);
}

void free_ljob(struct ljob_t *job_tab) {
    free(job_tab->job);
    *job_tab = null_ljob;
}
