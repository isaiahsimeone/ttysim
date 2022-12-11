#define _GNU_SOURCE

#include "ttysim.h"

pthread_mutex_t recording_lock;
struct termios  original_term;
pid_t           child_pid;
int             master;
int             slave;
int             out_file;
int             tmp_file;
struct timespec recording_start_time;

/*
 * Usage: ./ttysim [output_file_name]
 */
int main(int argc, char** argv) {
    struct termios termset;
    struct winsize winsize;

    char* output_file_name;
    char* shell = getenv("SHELL");

    if (!shell) {
        warn("SHELL is not defined. Defaulting to /bin/bash");
        shell = "/bin/bash";
    }

    /* Set output filename */
    if (argc >= 2)
        output_file_name = argv[1];
    else
        get_date_time_string_now(&output_file_name);

    /* Create/Open file */
  //  if ((out_file = open(output_file_name, O_CREAT | O_RDWR) == -1))
  //      fatal("Unable to open output file");

    printf("OUTPUT STRING = %s\n", output_file_name);

    /* Create working /tmp file */
    char name_buf[TMP_NAME_LEN];
    strncpy(name_buf, "/tmp/ttysimXXXXXX", TMP_NAME_LEN);
    if ((tmp_file = mkstemp(name_buf)) == -1)
        fatal("Unable to create /tmp file for recording");

    /* New pseudo-terminal master */
    if ((master = getpt()) == -1)
        fatal("call to getpt() failed");

    /* Initialise recording_start_time */
    clock_gettime(CLOCK_MONOTONIC, &recording_start_time);

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
            /* Not reached */
        default: /* Parent */
            pthread_create(&tid_stdout, 0, stdout_monitor, NULL);
            pthread_create(&tid_stdin, 0, stdin_monitor, NULL);
    }

    pthread_join(tid_stdout, 0);
    pthread_join(tid_stdin, 0);

    return EXIT_SUCCESS;
}

/*
 * Plumb and spawn the specified shell
 */
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

/*
 * Get stdin from the parent and pass to the slave terminal
 */
void* stdin_monitor(void* targs) {
    printf("stdin monitor up\n");

    close(slave);

    EventHeader* header = malloc(sizeof(EventHeader));
    memset(header, 0, sizeof(EventHeader));

    int read_sz;
    char input[BUFFER_SZ];
    while ((read_sz = read(STDIN_FILENO, input, BUFFER_SZ)) > 0) {
        pthread_mutex_lock(&recording_lock);
        generate_header(header, read_sz, STDIN_FILENO);
        write(master, input, read_sz);
        /* Write header to tmp file */
        write(tmp_file, header, sizeof(EventHeader));
        /* Write content to tmp file */
        write(tmp_file, input, read_sz);
        pthread_mutex_unlock(&recording_lock);
        memset(input, 0, BUFFER_SZ);
    }

    return NULL; /* Satisfy GCC */
}

/*
 * Write the slave terminal's output to our (the parents)
 * stdout stream, then write to the recording file.
 */
void* stdout_monitor(void* targs) {
    printf("stdout monitor up\n");

    close(slave);

    EventHeader* header = malloc(sizeof(EventHeader));
    memset(header, 0, sizeof(EventHeader));

    int read_sz;
    char output[BUFFER_SZ];
    while ((read_sz = read(master, output, BUFFER_SZ)) > 0) {
        pthread_mutex_lock(&recording_lock);
        generate_header(header, read_sz, STDOUT_FILENO);
        write(STDOUT_FILENO, output, read_sz);
        /* Write header to tmp */
        write(tmp_file, header, sizeof(EventHeader));
        /* Write content to tmp */
        write(tmp_file, output, read_sz);
        pthread_mutex_unlock(&recording_lock);
        memset(output, 0, BUFFER_SZ);
    }

    printf("Stdout monitor exiting\n");

    return NULL; /* Satisfy GCC */
}

/*
 * Wait for the child pty host to die.
 */
void child_signal_handler() {
    int stat;
    if (waitpid(child_pid, &stat, WNOHANG) == child_pid)
        finish_recording();
}

/*
 * Save the recorded session to disk. Restore the original terminal
 * parameters & exit.
 */
void finish_recording() {
    // Save recording
    fsync(tmp_file);
    close(tmp_file);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_term);
    exit(EXIT_SUCCESS);
}

int generate_header(EventHeader* eh, uint64_t len, int stream) {
    memset(eh, 0, sizeof(EventHeader));
    eh->len = len;
    eh->time = ms_since_recording_began();
    eh->stream = stream;
    return 1;
}

uint64_t ms_since_recording_began() {
    // Get the current time using the clock_gettime() function
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    // Calculate the elapsed time in milliseconds
    uint64_t elapsed_time_ms = (current_time.tv_sec - recording_start_time.tv_sec) * 1000 +
                               (current_time.tv_nsec - recording_start_time.tv_nsec) / 1000000;

    return elapsed_time_ms;
}

/*
 * Create a date+time string with the current date+time
 * Returned format is YYYY-MM-DD_HHmmss
 */
void get_date_time_string_now(char** dest) {
    int str_len = 18; //strlen("YYYY-MM-DD_HHmmss") + 1;
    time_t t_now = time(NULL);
    struct tm tm = *localtime(&t_now);
    char* dt_string = malloc(sizeof(char) * str_len);
    memset(dt_string, 0, sizeof(char) * str_len);

    if (snprintf(dt_string, str_len, "%04d-%02d-%02d_%02d%02d%02d", tm.tm_year + 1900,
                 tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec) < 0)
        strncpy(dt_string, "ttysim.out", str_len); /* Fallback name */

    *dest = dt_string;
}

/*
 * Fatal error
 */
void fatal(const char* msg) {
    fprintf(stderr, COL_RED "Error: " COL_RESET "%s (%s)\n", msg, strerror(errno));
    kill(child_pid, SIGINT);
    kill(0, SIGINT);
}

/*
 * Pretty warning
 */
void warn(const char* msg) {
    fprintf(stderr, COL_YELLOW "Warning: " COL_RESET "%s\n", msg);
}