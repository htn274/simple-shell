#ifndef _ALIAS_
#define _ALIAS_

int set_alias(const char *alias, const char * cmd);
int unset_alias(const char *alias);
const char *find_alias(const char *alias);
char *replace_env_var(char *s);

#endif