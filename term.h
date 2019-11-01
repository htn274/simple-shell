#ifndef _TERM_
#define _TERM_
#include <signal.h>

//set control for process group, and wether we should catch SIGTTOU and SIGTTIN
void set_control(pid_t pgrp, int catch_sig);

#endif