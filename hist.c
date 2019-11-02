#include <stdlib.h>
#include <string.h>
#include "hist.h"

static int n_hist = 0;
static char **hist = NULL;

int hist_len() {
    return n_hist;
}

void add_history(const char *cmd) {
    ++n_hist;
    hist = realloc(hist, n_hist * sizeof(hist[0]));

    hist[n_hist - 1] = strdup(cmd);
}
const char *get_history(int id) {
    if (id < 0 || id >= n_hist)
        return NULL;
    
    return hist[id];
}