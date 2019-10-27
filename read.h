#ifndef _READ_
#define _READ_


#define SHELL_NAME "\x1B[38;5;2m\x1B[1mosh>\x1B[0m "

void new_line();
void print_prompt();

char *read_cmd();
void clear_buffer();

#endif