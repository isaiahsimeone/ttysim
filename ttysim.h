#ifndef TTYSIM_TTYSIM_H
#define TTYSIM_TTYSIM_H

#define COL_YELLOW      "\x1b[33m"
#define COL_RED         "\x1b[31m"
#define COL_RESET       "\x1b[0m"

#define BUFFER_SZ       4096
#define TMP_NAME_LEN    18

#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pty.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    uint32_t    len;    /* Length of event data which follows header - max event length ~4 billion chars */
    uint32_t    time;   /* Time of event in ms (since recording began) - Max recording ~49 days */
    int         stream; /* STDIN/STDOUT */
} EventHeader;

void*               stdin_monitor(void*);
void*               stdout_monitor(void*);
void                spawn_terminal(char*);
void                child_signal_handler(void);
void                finish_recording(void);
void                fatal(const char*);
void                warn(const char*);
void                get_date_time_string_now(char**);
int                 generate_header(EventHeader*, uint64_t, int);
uint64_t            ms_since_recording_began(void);

#endif /* TTYSIM_TTYSIM_H */
