#ifndef _HIST_
#define _HIST_

int hist_len();
void add_history(char *cmd);
const char *get_history(int id);


#endif