#ifndef TTYSIM_TTYSIM_H
#define TTYSIM_TTYSIM_H

#define COL_YELLOW  "\x1b[33m"
#define COL_RED     "\x1b[31m"
#define COL_RESET   "\x1b[0m"

#define BUFFER_SZ   4096

#include <sys/wait.h>

#include <errno.h>
#include <pthread.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void*               stdin_monitor(void*);
void*               stdout_monitor(void*);
void                spawn_terminal(char*);
void                child_signal_handler(void);
void                finish_recording(void);
void                fatal(const char*);
void                warn(const char*);

#endif /* TTYSIM_TTYSIM_H */
