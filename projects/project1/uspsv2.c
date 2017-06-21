/*
 * uspsv2.c
 * Author: Cole Vikupitz (cvikupit)
 * CIS 415 - Project 1
 *
 * Second version of the USPS. Same as the first version, but now halts each
 * child process before execvp() using nanosleep. Sends a SIGUSR1 signal to
 * each process, followed by SIGSTOP, followed by SIGCONT.
 *
 * This is my own work, with the exception of the functions used in p1fxns.c/h
 * provided by Joe Sventek. Other sections are also borrowed from the LPE book
 * and Piazza posts on signal handling. I have referenced each of these sections
 * as best I can.
 */

#include <fcntl.h>              /* Used for open() */
#include <signal.h>             /* Used for signal() */
#include <stdlib.h>             /* Used for getenv(), free(), NULL */
#include <sys/stat.h>           /* Needed for open() on some UNIX distributions */
#include <sys/types.h>          /* Needed for open() on some UNIX distributions */
#include <sys/wait.h>           /* Used for wait() */
#include <time.h>               /* Used for nanosleep() */
#include <unistd.h>             /* Used for fork(), execvp(), _exit() */
#include "clist.h"              /* CList ADT */
#include "process.h"            /* Process ADT */
#include "p1fxns.h"             /* Used for p1strcpy(), p1strcat(), p1strneq(), ... */

/* Macro used to suppress compiler warnings */
#define UNUSED __attribute__((unused))

/* Variable used for halting child processes */
static volatile int interrupted = 0;

/* Circular list that stores the processes to run */
static CList *pr_list = NULL;

/* Method signatures */
/* Populates the queue with processes given a file descriptor to read from */
static void load_processes(int fd);
/* Invoked with SIGUSR1, sends waking signal to child process */
static void sigusr1_handler(int signo);
/* Frees the program's reserved memory */
static void free_mem(void);
/* Prints the given message and exits the program */
static void print_error(char *msg);


/*
 * Runs the program.
 */
int main(int argc, char **argv) {

    Process **prs = NULL;
    char *qu_str = NULL;
    char *file = NULL;
    char buffer[4096];
    char **args = NULL;
    int i, fd = STDIN_FILENO;
    long j, len = 0L;
    pid_t pid;

    /* Establish the SIGUSR1 signal, print error if fails */
    if (signal(SIGUSR1, sigusr1_handler) == SIG_ERR) {
        p1strcpy(buffer, "ERROR: Failed to establish SIGUSR1 signal.");
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
            /* Parent process, assign the PID */
            assign_pid(prs[j], pid);
        } else {
            /* Fork error, print error and exit */
            p1strcpy(buffer, "ERROR: Previous call to fork() failed.");
            print_error(buffer);
        }
    }

    /* Send each process SIGUSR1 signal, wakes them up */
    for (j = 0L; j < len; j++)
        kill(pr_pid(prs[j]), SIGUSR1);

    /* Send each process SIGSTOP signal, stop running process */
    for (j = 0L; j < len; j++)
        kill(pr_pid(prs[j]), SIGSTOP);

    /* Send each process SIGCONT signal, resume running process */
    for (j = 0L; j < len; j++)
        kill(pr_pid(prs[j]), SIGCONT);

    /* Wait for each process to complete */
    for (j = 0L; j < len; j++) {
        pid = pr_pid(prs[j]);
        wait(&pid);
    }

    /* Free all allocated memory */
    free(prs);
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
 * SIGUSR1 handler; increments the interrupted variable, wakes the child process up.
 */
static void sigusr1_handler(UNUSED int signo) {

    interrupted++;
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

