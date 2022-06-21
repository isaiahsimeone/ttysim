#define _GNU_SOURCE


#include "ttysim.h"


pthread_mutex_t recording_lock;
int master;
int slave;
pid_t child_pid;
struct termios original_term;

int main(int argc, char** argv) {
    char* shell = getenv("SHELL");
    if (!shell) {
        warn("SHELL is not defined. Defaulting to /bin/bash");
        shell = "/bin/bash";
    }

    struct termios termset;
    struct winsize winsize;

    /* New pseudoterminal master */
    if ((master = getpt()) < 0)
        fatal("call to getpt() failed");

    /* Query term parameters */
    tcgetattr(STDIN_FILENO, &original_term);
    ioctl(STDIN_FILENO, TIOCGWINSZ, (char*)&winsize);

    /* Set term parameters */
    termset = original_term;
    cfmakeraw(&termset); /* Raw mode - no echo */
    termset.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &termset);


    if (openpty(&master, &slave, NULL, &original_term, &winsize) == -1)
        fatal("openpty() failed");

    /* Listen for child death */
    signal(SIGCHLD, (__sighandler_t) child_signal_handler);

    pthread_mutex_init(&recording_lock, NULL);

    pthread_t tid_stdout, tid_stdin;
    switch ((child_pid = fork())) {
        case 0: /* Child */
            usleep(1000); // Let the monitors come up
            spawn_terminal(shell);
            /* Not reached */
        case -1:
            fatal("Failed to fork");
        default: /* Parent */
            pthread_create(&tid_stdout, 0, stdout_monitor, NULL);
            pthread_create(&tid_stdin, 0, stdin_monitor, NULL);
    }

    pthread_join(tid_stdout, 0);
    pthread_join(tid_stdin, 0);

    return EXIT_SUCCESS;
}

void spawn_terminal(char* shell) {
    setsid();
    ioctl(slave, TIOCSCTTY, 0);

    close(master);
    dup2(slave, STDIN_FILENO);
    dup2(slave, STDOUT_FILENO);
    dup2(slave, STDERR_FILENO);
    close(slave);

    /* Spawn shell in interactive mode */
    execl(shell, "-i", 0);
    fatal("execl() failure");
}

void* stdin_monitor(void* targs) {
    printf("stdin monitor up\n");

    close(slave);

    int read_sz;
    char input[BUFFER_SZ];
    while ((read_sz = read(STDIN_FILENO, input, BUFFER_SZ)) > 0) {
        pthread_mutex_lock(&recording_lock);
        write(master, input, read_sz);
       // printf("%s", input);
        pthread_mutex_unlock(&recording_lock);
        memset(input, 0, BUFFER_SZ);
    }

    return NULL; /* Not reached */
}

void* stdout_monitor(void* targs) {
    printf("stdout monitor up\n");

    close(slave);

    int read_sz;
    char output[BUFFER_SZ];
    while ((read_sz = read(master, output, BUFFER_SZ)) > 0) {
        pthread_mutex_lock(&recording_lock);
        write(STDOUT_FILENO, output, read_sz);
        //printf("%s", output);
        pthread_mutex_unlock(&recording_lock);
        memset(output, 0, BUFFER_SZ);
    }

    printf("Stdout monitor exiting\n");

    return NULL; /* Satisfy GCC */
}


void child_signal_handler() {
    int stat;
    if (waitpid(child_pid, &stat, WNOHANG) == child_pid)
        finish_recording();
}

void finish_recording() {
    // Save recording

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_term);
    exit(EXIT_SUCCESS);
}


void fatal(const char* msg) {
    fprintf(stderr, COL_RED "Error: " COL_RESET "%s (%s)\n", msg, strerror(errno));
    kill(child_pid, SIGINT);
    kill(0, SIGINT);
}

void warn(const char* msg) {
    fprintf(stderr, COL_YELLOW "Warning: " COL_RESET "%s\n", msg);
}