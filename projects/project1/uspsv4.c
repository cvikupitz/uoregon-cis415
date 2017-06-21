/*
 * uspsv4.c
 * Author: Cole Vikupitz (cvikupit)
 * CIS 415 - Project 1
 *
 * Fourth version of the USPS. Same as v3, but now access process information in
 * /proc/<pid>/ and displays it for the user.
 *
 * This is my own work, with the exception of the functions used in p1fxns.c/h
 * provided by Joe Sventek. In addition, some code sections have been borrowed
 * from the LPE book as well, specifically sections 8.4.5-6 where the timer and
 * child handlers are implemented.
 *
 * UPDATE: Previously, only the pid was printed, but I have now successfully printed
 * out more information as seen in the solution. Prints out the PID, status, cmd,
 * page faults, VM size and resident set size, and the number of system read and write
 * calls, also as shown in the annotated solution.
 */

#include <fcntl.h>              /* Used for open() */
#include <signal.h>             /* Used for signal() */
#include <stdlib.h>             /* Used for getenv(), free(), NULL */
#include <sys/stat.h>           /* Needed for open() on some UNIX distributions */
#include <sys/time.h>           /* Used for setitimer(), struct itimerval */
#include <sys/types.h>          /* Needed for open() on some UNIX distributions */
#include <sys/wait.h>           /* Used for wait() */
#include <time.h>               /* Used for nanosleep() */
#include <unistd.h>             /* Used for fork(), execvp(), _exit() */
#include "clist.h"              /* CList ADT */
#include "process.h"            /* Process ADT */
#include "p1fxns.h"             /* Used for p1strcpy(), p1strcat(), p1strneq(), ... */

/* Macro used to suppress compiler warnings */
#define UNUSED __attribute__((unused))
/* Minimum quantum (in ms) allowed */
#define MIN_QUANTUM 100
/* Maximum quantum (in ms) allowed */
#define MAX_QUANTUM 1000
/* The time slice (in ms) for each quantum tick */
#define SLICE 20

/* Variable used for halting child processes */
static volatile int interrupted = 0;

/* Number of active child processes remaining */
static volatile long active_processes = 0L;

/* The PID of the parent process */
static pid_t parent_pid = 0;

/* Circular list that stores the processes to run */
static CList *pr_list = NULL;

/* Method signatures */
/* Populates the queue with processes given a file descriptor to read from */
static void load_processes(int fd);
/* Kills the child process, sets its status to DEAD */
static void kill_process(pid_t pid);
/* Invoked with SIGUSR1, sends waking signal to child process */
static void sigusr1_handler(int signo);
/* Invoked with SIGUSR2, does nothing */
static void sigusr2_handler(int signo);
/* Invoked with SIGCHLD, kills the child process and wakes the parent process */
static void sigchld_handler(int signo);
/* Invoked with SIGALRM, selects next process for execution */
static void sigalrm_handler(int signo);
/* Compacts large number strings down w/ abbreviations  */
static void compact_num(char *num);
/* Comapcts large byte strings down w/ abbreviations */
static void compact_size(char *bytes);
/* Converts a string into a long */
static long p1atol(char *s);
/* Converts a long into a string */
static void p1ltoa(long number, char *buf);
/* Converts clock ticks in string into seconds */
static void ticks_to_sec(char *ticks);
/* Converts pages in string to total bytes */
static void pages_to_bytes(char *buff);
/* Prints out process information by accessing files in the proc directory */
static void print(Process *pr);
/* Frees the program's reserved memory */
static void free_mem(void);
/* Prints the given message and exits the program */
static void print_error(char *msg);


/*
 * Runs the program.
 */
int main(int argc, char **argv) {

    Process *temp, **prs = NULL;
    struct itimerval timer;
    char *qu_str = NULL;
    char *file = NULL;
    char buffer[4096];
    char **args = NULL;
    int i, quantum, nticks, fd = STDIN_FILENO;
    long j, len = 0L;
    pid_t pid;

    /* Establish the SIGUSR1 signal, print error if fails */
    if (signal(SIGUSR1, sigusr1_handler) == SIG_ERR) {
        p1strcpy(buffer, "ERROR: Failed to establish SIGUSR1 signal.");
        print_error(buffer);
    }

    /* Establish the SIGUSR2 signal, print error if fails */
    if (signal(SIGUSR2, sigusr2_handler) == SIG_ERR) {
        p1strcpy(buffer, "ERROR: Failed to establish SIGUSR2 signal.");
        print_error(buffer);
    }
    parent_pid = getpid();

    /* Establish the SIGCHLD signal, print error if fails */
    if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
        p1strcpy(buffer, "ERROR: Failed to establish SIGCHLD signal.");
        print_error(buffer);
    }

    /* Establish the SIGALRM signal, print error if fails */
    if (signal(SIGALRM, sigalrm_handler) == SIG_ERR) {
        p1strcpy(buffer, "ERROR: Failed to establish SIGALRM signal.");
        print_error(buffer);
    }

    /* Create the process queue, print error if allocation fails */
    pr_list = cl_create();
    if (pr_list == NULL) {
        p1strcpy(buffer, "ERROR: Failed to allocate sufficient amount of memory.");
        print_error(buffer);
    }

    /* Attempt to extract from environment variable */
    qu_str = getenv("USPS_QUANTUM_MSEC");

    /* Iterate arguments, extract quantum and work file */
    for (i = 1; i < argc; i++) {
        /* Quantum specified with flag */
        if (p1strneq(argv[i], "--quantum=", 10))
            qu_str = (argv[i] + 10);
        /* Other argument assumed to be the workload file */
        else
            file = argv[i];
    }

    /* Quantum undefined at this point, print error and exit */
    if (qu_str == NULL) {
        p1strcpy(buffer, "ERROR: Quantum undefined, define through argument or env var 'USPS_QUANTUM_MSEC'.\n");
        p1strcat(buffer, "Usage: ");
        p1strcat(buffer, argv[0]);
        p1strcat(buffer, " [--quantum=<msec>] [workload_file]");
        print_error(buffer);
    }

    /* Assert that the quantum is not less than minimum allowed */
    quantum = p1atoi(qu_str);
    if (quantum < MIN_QUANTUM) {
        p1putstr(STDOUT_FILENO, "The specified quantum is less than the minimum (");
        p1putint(STDOUT_FILENO, MIN_QUANTUM);
        p1putstr(STDOUT_FILENO, "), setting to minimum.\n");
        quantum = MIN_QUANTUM;
    }

    /* Assert that the quantum does not exceed maximum allowed */
    if (quantum > MAX_QUANTUM) {
        p1putstr(STDOUT_FILENO, "The specified quantum is greater than the maximum (");
        p1putint(STDOUT_FILENO, MAX_QUANTUM);
        p1putstr(STDOUT_FILENO, "), setting to maximum.\n");
        quantum = MAX_QUANTUM;
    }

    /* Round quantum to nearest 100, calculate ticks per time quantum */
    quantum = (((quantum + 50) / 100) * 100);
    nticks = (quantum / SLICE);

    /* If a file has been specified, attempt to open it */
    if (file != NULL) {
        if ((fd = open(file, O_RDONLY)) < 0) {
            /* Failed to open file, print error and exit */
            p1strcpy(buffer, "ERROR: Failed to open: ");
            p1strcat(buffer, file);
            print_error(buffer);
        }
    }

    /* Load the queue with the processes, given the workload file */
    load_processes(fd);

    /* Get array of processes to fork from */
    if ((prs = (Process **)cl_toArray(pr_list, &len)) == NULL) {
        /* Allocation fails, print error and exit */
        if (!cl_isEmpty(pr_list)) {
            p1strcpy(buffer, "ERROR: Failed to allocate sufficient amount of memory.");
            print_error(buffer);
        }
    }
    active_processes = len;

    /*
     * For each process in the queue:
     *   Invoke fork(), obtain the PID
     *   For the parent, assign the PID to the process
     *   For the child, invoke execvp() on program
     *   Otherwise, print error and exit
     */
    for (j = 0; j < len; j++) {
        pid = fork();
        if (pid == 0) {
            /* The child waits here until SIGUSR1 is */
            /* This portion was borrowed from the Piazza post on signal handling */
            struct timespec timer = {0, 20000000};
            while (!interrupted)
                nanosleep(&timer, NULL);
            /* Child process, invoke execvp() on program */
            args = pr_argv(prs[j]);
            execvp(args[0], args);
            /* If this is reached, error occured invoking the program */
            /* Print error message and exit */
            p1strcpy(buffer, "ERROR: Failed to execute:");
            for (i = 0; args[i] != NULL; i++) {
                p1strcat(buffer, " ");
                p1strcat(buffer, args[i]);
            }
            free(prs);
            print_error(buffer);
        } else if (pid > 0) {
            /* Parent process, assign the PID and ticks per quantum */
            assign_pid(prs[j], pid);
            assign_ticks(prs[j], nticks);
        } else {
            /* Fork error, print error and exit */
            p1strcpy(buffer, "ERROR: Previous call to fork() failed.");
            print_error(buffer);
        }
    }
    /* Array not needed at this point, free it */
    free(prs);

    /* Sets the timer structs members */
    timer.it_value.tv_sec = (SLICE / 1000);
    timer.it_value.tv_usec = ((SLICE * 1000) % 1000000);
    timer.it_interval = timer.it_value;

    /* Timer is to run in real time, sends SIGALRM at each expiration */
    if ((setitimer(ITIMER_REAL, &timer, NULL)) == -1) {
        /* Timer set failed, kill all child processes */
        while (cl_remove(pr_list, (void **)&temp)) {
            kill(pr_pid(temp), SIGKILL);
            free_pr(temp);
        }
        /* Print error and exit */
        p1strcpy(buffer, "ERROR: Failed to create the quantum timer.");
        print_error(buffer);
    }

    /*
     * The first process will immediately halt, due to SIGALRM handler being
     * invoked. This means the second is the first to execute. To fix this,
     * rotate the list size()-1 times such that the last process is halted
     * immediately, and the first process will be the first to execute.
     */
    for (j = 0L; j < cl_size(pr_list) - 1; j++)
        cl_rotate(pr_list);

    /* Parent process pauses, waits for all child processes to complete */
    sigalrm_handler(SIGALRM);
    while (active_processes > 0L)
        pause();

    /* Free all allocated memory */
    free_mem();

    return 0;
}

/*
 * Populates the process list with process instances given a file descriptor of
 * the work file to read from. Reads each line, creates a process struct from the
 * line, and adds it into the queue. Closes the descriptor when its done.
 */
static void load_processes(int fd) {

    Process *pr;
    char buffer[4096], word[1024];
    int len;

    while (p1getline(fd, buffer, sizeof(buffer)) > 0) {

        /* Ignores blank lines */
        p1getword(buffer, 0, word);
        if (p1strneq(word, "\n", 1))
            continue;
        
        /* Trim off newline */
        len = p1strlen(buffer);
        if (buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';

        /* Creates the process from the given line */
        pr = malloc_pr(buffer);
        if (pr == NULL) {
            /* Allocation failed, print error and exit */
            if (fd != STDIN_FILENO)
                close(fd);
            p1strcpy(buffer, "ERROR: Failed to allocate sufficient amount of memory");
            print_error(buffer);
        }

        /* Insert the new struct instance into list */
        if (!cl_insert(pr_list, pr)) {
            /* Insertion failed, print error and exit */
            if (fd != STDIN_FILENO)
                close(fd);
            free_pr(pr);
            p1strcpy(buffer, "ERROR: Failed to allocate sufficient amount of memory");
            print_error(buffer);
        }
    }

    /* File no longer needed, close it */
    if (fd != STDIN_FILENO)
        close(fd);
}

/*
 * Kills the child process with the specified PID. Sets its status
 * to DEAD, such that, it will be removed from the queue when its next
 * selected by the scheduler to run.
 */
static void kill_process(pid_t pid) {

    long i, len = 0L;
    Process **plist = NULL;

    /* Get current array of processes to iterate through */
    if ((plist = (Process **)cl_toArray(pr_list, &len)) == NULL)
        return;

    /* Iterates through array, finds the child process */
    for (i = 0L; i < len; i++) {
        if (pr_pid(plist[i]) == pid) {
            /* Process found, sets its status to DEAD */
            pr_kill(plist[i]);
            break;
        }
    }

    /* Free array when done */
    free(plist);
}

/*
 * SIGUSR1 handler; increments the interrupted variable, wakes the child process up.
 */
static void sigusr1_handler(UNUSED int signo) {

    interrupted++;
}

/*
 * SIGUSR2 handler; does nothing.
 */
static void sigusr2_handler(UNUSED int signo) {}

/*
 * SIGCHLD handler; kills the child process so that it can be removed from the queue later
 * on. Also wakes up parent process from pause(), checks remaining processes left.
 */
static void sigchld_handler(UNUSED int signo) {

    pid_t pid;
    int status;

    /* This section was borrowed from the LPE book, section 8.4.6 */
    /* Decrement the number of active processes and wake up the parent process's pause() */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            kill_process(pid);
            active_processes--;
            kill(parent_pid, SIGUSR2);
        }
    }
}

/*
 * SIGALRM handler; when the timer expires, selects the next process in the queue to
 * run.
 */
static void sigalrm_handler(UNUSED int signo) {

    Process *temp;

    /*
     * First, check to see if there are ticks left in the time
     * quantum. If so, allow process to continue running. Otherwise,
     * send SIGSTOP to the child process.
     */
    if (!cl_head(pr_list, (void **)&temp))
        return;
    if (pr_status(temp) == ALIVE) {
        if (decr_tick(temp))
            return;
        kill(pr_pid(temp), SIGSTOP);
        /* Print process information */
        print(temp);
    }

    /* Rotate list to get the next process */
    cl_rotate(pr_list);

    /*
     * Now, check the status of the next process. If it hasn't
     * started yet, send it a SIGUSR1 signal. If it has, send it
     * a SIGCONT signal. Otherwise, it is assumed to have finished,
     * so remove it from the queue.
     */
    while (cl_head(pr_list, (void **)&temp)) {
        switch (pr_status(temp)) {
            /* Waiting to start, send it SIGUSR1 signal */
            case WAITING:
                pr_wake(temp);
                kill(pr_pid(temp), SIGUSR1);
                return;
            /* Started already, send it SIGCONT signal */
            case ALIVE:
                kill(pr_pid(temp), SIGCONT);
                return;
            /* Finished or died, remove from queue */
            case DEAD:
            default:
                cl_remove(pr_list, (void **)&temp);
                free_pr(temp);
        }
    }
}

/*
 * Compacts the specified string containing a large number down to a compact
 * version with an abbreviation. For example, the number '1000' is converted
 * into 1K, '1000000' is converted into 1M, '1000000000' is converted into 1B,
 * and so on.
 */
static void compact_num(char *num) {

    int len = p1strlen(num);
    if (len >= 16) {
        /* Shorten to quadrillion */
        num[len - 15] = 'Q';
        num[len - 14] = '\0';
    } else if (len >= 13) {
        /* Shorten to trillion */
        num[len - 12] = 'T';
        num[len - 11] = '\0';
    } else if (len >= 10) {
        /* Shorten to billion */
        num[len - 9] = 'B';
        num[len - 8] = '\0';
    } else if (len >= 7) {
        /* Shorten to million */
        num[len - 6] = 'M';
        num[len - 5] = '\0';
    } else if (len >= 4) {
        /* Shorten to thousand */
        num[len - 3] = 'K';
        num[len - 2] = '\0';
    }
}

/*
 * Compacts the specified string containing a large byte size down to a compact
 * version with an abbreviation. For example, the number '1000' is converted
 * into 1 Kb, '1000000' is converted into 1 Mb, '1000000000' is converted into
 * 1 Gb, and so on.
 */
static void compact_size(char *bytes) {

    int len = p1strlen(bytes);
    if (len >= 13) {
        /* Convert to terabytes */
        bytes[len - 12] = ' ';
        bytes[len - 11] = 'T';
        bytes[len - 10] = 'b';
        bytes[len - 9] = '\0';
    } else if (len >= 10) {
        /* Convert to gigabytes */
        bytes[len - 9] = ' ';
        bytes[len - 8] = 'G';
        bytes[len - 7] = 'b';
        bytes[len - 6] = '\0';
    } else if (len >= 7) {
        /* Convert to megabytes */
        bytes[len - 6] = ' ';
        bytes[len - 5] = 'M';
        bytes[len - 4] = 'b';
        bytes[len - 3] = '\0';
    } else if (len >= 4) {
        /* Convert to kilobytes */
        bytes[len - 3] = ' ';
        bytes[len - 2] = 'K';
        bytes[len - 1] = 'b';
        bytes[len - 0] = '\0';
    }
}

/*
 * Converts the specified sting into a long number, and returns it.
 */
static long p1atol(char *s) {

    long ans;

    for (ans = 0L; *s >= '0' && *s <= '9'; s++)
        ans = 10 * ans + (long) (*s - '0');
    return ans;
}

/*
 * Converts the specified long number into a string.
 */
static void p1ltoa(long number, char *buf) {

    char tmp[25];
    long n, i, negative;
    static char digits[] = "0123456789";

    if (number == 0L) {
        tmp[0] = '0';
        i = 1;
    } else {
        if ((n = number) < 0L) {
            negative = 1;
            n = -n;
        } else
            negative = 0;
        for (i = 0; n != 0L; i++) {
            tmp[i] = digits[n % 10];
            n /= 10;
        }
        if (negative) {
            tmp[i] = '-';
            i++;
        }
    }
    while (--i >= 0)
       *buf++ = tmp[i];
    *buf = '\0';
}

/*
 * Converts the string containing the number of clock ticks into seconds, then
 * compacts the number if necessary.
 */
static void ticks_to_sec(char *buff) {

    /* Converts into seconds */
    unsigned long ticks = p1atol(buff);
    unsigned long ticks_per_sec = sysconf(_SC_CLK_TCK);
    unsigned long sec = (ticks / ticks_per_sec);
    /* Convert back to string, copy back into buffer */
    p1ltoa(sec, buff);
    compact_num(buff);
}

/*
 * Converts the string containing the number of pages into bytes, then compacts
 * the number if necessary.
 */
static void pages_to_bytes(char *buff) {

    /* Converts pages into bytes */
    unsigned long pages = p1atoi(buff);
    unsigned long bytes_per_page = sysconf(_SC_PAGESIZE);
    unsigned long bytes = (pages * bytes_per_page);
    /* Convert back to string, copy back into buffer */
    p1ltoa(bytes, buff);
    compact_size(buff);
}

/*
 * Prints out information on the specified process by accesssing its files
 * from within the /proc/<pid>/ directory.
 */
#define LIMIT 20L       /* Prints the header after this many lines */
static void print(Process *pr) {

    static unsigned long iter = 0;
    int i, index;
    int cmd_fd = -1, io_fd = -1, stat_fd = -1;
    char pid_str[32], syscr[32], syscw[32], stat[32], flts[32],
        usrtm[32], systm[32], vmsz[32], rssz[32], cmd[2048];
    char res[4096], buffer[4096], word[1024];
    char *res_ptr = NULL;

    /* Access the /proc/<pid>/cmdline file */
    p1itoa((int)pr_pid(pr), pid_str);
    p1strcpy(buffer, "/proc/");
    p1strcat(buffer, pid_str);
    p1strcat(buffer, "/cmdline");
    if ((cmd_fd = open(buffer, O_RDONLY)) < 0)
        goto close_files;

    /* Store the cmd into a buffer for later */
    p1getline(cmd_fd, cmd, sizeof(cmd));
    i = 0;
    while (1) {
        if (cmd[i] == '\0') {
            i++;
            /* Extract all args from cmdline; replace terminating chars with space */
            if (cmd[i] == '\0')
                break;
            cmd[--i] = ' ';
        }
        i++;
    }
    close(cmd_fd);
    cmd_fd = -1;

    /* Accesses the /proc/<pid>/io file */
    p1strcpy(buffer, "/proc/");
    p1strcat(buffer, pid_str);
    p1strcat(buffer, "/io");
    if ((io_fd = open(buffer, O_RDONLY)) < 0)
        goto close_files;
    i = 1;

    /* Only want to extract the system read and write calls; need to skip some lines */
    while (p1getline(io_fd, buffer, sizeof(buffer))) {
        index = 0;
        index = p1getword(buffer, index, word);
        if (i == 3) {
            /* On the 3rd line is the system read calls */
            index = p1getword(buffer, index, syscr);
            syscr[p1strlen(syscr)-1] = '\0';
            compact_num(syscr);
        }
        if (i == 4) {
            /* On the 4th line is the system write calls */
            index = p1getword(buffer, index, syscw);
            syscw[p1strlen(syscw)-1] = '\0';
            compact_num(syscw);
        }
        i++;
    }
    close(io_fd);
    io_fd = -1;

    /* Accesses the /proc/<pid>/stat file */
    p1strcpy(buffer, "/proc/");
    p1strcat(buffer, pid_str);
    p1strcat(buffer, "/stat");
    if ((stat_fd = open(buffer, O_RDONLY)) < 0)
        goto close_files;

    /* All information is on one line; need to extract by word */
    p1getline(stat_fd, buffer, sizeof(buffer));
    i = 1;
    index = 0;
    while ((index = p1getword(buffer, index, word)) != -1) {
        switch (i) {
            /* Obtains the current status of the process */
            case 3:
                p1strcpy(stat, word);
                break;
            /* Obtains number of major faults the process has made */
            case 12:
                p1strcpy(flts, word);
                compact_num(flts);
                break;
            /* Obtains amount of time in user mode (in clock ticks) */
            case 14:
                p1strcpy(usrtm, word);
                ticks_to_sec(usrtm);
                break;
            /* Obtains amount of time in kernel mode (in clock ticks) */
            case 15:
                p1strcpy(systm, word);
                ticks_to_sec(systm);
                break;
            /* Obtains the virtual memory size (in bytes) */
            case 23:
                p1strcpy(vmsz, word);
                compact_size(vmsz);
                break;
            /* Obtains the resident set size (in bytes) */
            case 24:
                p1strcpy(rssz, word);
                pages_to_bytes(rssz);
                break;
            /* All other info useless, ignore */
            default:
                break;
        }
        i++;
    }
    close(stat_fd);
    stat_fd = -1;

    /* Redisplay the header when needed */
    if (!iter)
        p1putstr(STDOUT_FILENO, "PID      SysCR   SysCW   State  Flts    UsrTm   SysTm   VMSz    RSSz    Cmd\n");
    /* Keep track how many lines printed so far, before reprinting header */
    if (++(iter) >= LIMIT)
        iter = 0;

    /* Pack into one string for printing, format with appropriate spacing */
    res_ptr = res;
    res_ptr = p1strpack(pid_str, 9, ' ', res_ptr);
    res_ptr = p1strpack(syscr, 8, ' ', res_ptr);
    res_ptr = p1strpack(syscw, 8, ' ', res_ptr);
    res_ptr = p1strpack(stat, 7, ' ', res_ptr);
    res_ptr = p1strpack(flts, 8, ' ', res_ptr);
    res_ptr = p1strpack(usrtm, 8, ' ', res_ptr);
    res_ptr = p1strpack(systm, 8, ' ', res_ptr);
    res_ptr = p1strpack(vmsz, 8, ' ', res_ptr);
    res_ptr = p1strpack(rssz, 8, ' ', res_ptr);
    res_ptr = p1strpack(cmd, 0, ' ', res_ptr);

    /* Prints out the process information */
    p1putstr(STDOUT_FILENO, res);
    p1putstr(STDOUT_FILENO, "\n");
    return;

close_files:
    /* Close any open files and return */
    if (cmd_fd != -1)
        close(cmd_fd);
    if (io_fd != -1)
        close(io_fd);
    if (stat_fd != -1)
        close(stat_fd);
    return;
}

/*
 * Frees all global allocated memory, returns it back to heap.
 */
static void free_mem(void) {

    if (pr_list != NULL)
        cl_destroy(pr_list, (void *)free_pr);
}

/*
 * Prints out specified error message and exits program.
 */
static void print_error(char *msg) {

    p1strcat(msg, "\n");
    p1putstr(STDOUT_FILENO, msg);
    /* Also must free all global memory */
    free_mem();
    _exit(0);
}

