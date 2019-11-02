#ifndef _HIST_
#define _HIST_

int hist_len();
void add_history(const char *cmd);
const char *get_history(int id);


#endif