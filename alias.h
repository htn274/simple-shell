#ifndef _ALIAS_
#define _ALIAS_

int get_alias_cnt();
int set_alias(const char *alias, const char * cmd);
int unset_alias(const char *alias);
int find_alias(const char *alias);
const char *get_alias_cmd(int id);
const char *get_alias(int id);

void set_alias_state(int id, int is_on);

#endif