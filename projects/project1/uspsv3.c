/*
 * uspsv3.c
 * Author: Cole Vikupitz (cvikupit)
 * CIS 415 - Project 1
 *
 * Third version of the USPS. Now uses RR technique to execute the child
 * processes. Each child process is allocated the given time quantum to run
 * on, then switches off to next process when done or quantum expires.
 *
 * This is my own work, with the exception of the functions used in p1fxns.c/h
 * provided by Joe Sventek. In addition, some code sections have been borrowed
 * from the LPE book as well, specifically sections 8.4.5-6 where the timer and
 * child handlers are implemented.
 *
 * UPDATE: I have added min/max time quantums; also rounds quantum to nearest 100,
 * and implements ticks as shown in the annotated solution.
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

