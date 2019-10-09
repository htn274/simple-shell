#ifndef _ALIAS_
#define _ALIAS_

int set_alias(const char *alias, const char * cmd);
int unset_alias(const char *alias);
int find_alias(const char *alias);
const char *get_alias(int id);

void set_alias_state(int id, int is_on);

char *replace_env_var(char *s);

#endif