/*
 * process.c
 * Author: Cole Vikupitz
 * CIS 415 - Project 1
 *
 * Source file for the Process ADT implementation. Contains an implementation
 * of a running process inside the process scheduler.
 *
 * This is my own work.
 *
 * UPDATE: I have now added quantum ticks into the implementation as shown in
 * the solution.
 */

#include <stdlib.h>     /* Used for malloc(), free() */
#include "process.h"    /* Process ADT */
#include "p1fxns.h"     /* Used for p1getword(), p1strlen(), p1strdup() */


/*
 * Struct representing the process ADT itself.
 */
struct process {
    char **argv;                /* Array of program arguments, ends with NULL */
    pid_t pid;                  /* The process PID */
    int status;                 /* The process's status; WAITING, ALIVE, or DEAD */
    int ticks;                  /* Quantum ticks left remaining */
    int nticks;                 /* Max quantum ticks allocated to this process */
    unsigned long prev_jfs;     /* Previous CPU jiffies */
    unsigned long curr_jfs;     /* Current CPU jiffies */
    unsigned long prev_util;    /* Previous utilization clokc ticks */
    unsigned long curr_util;    /* Current utilization clokc ticks */
};

/*
 * Local method to extract the program args from the given line.
 * Allocates a char ** array to store the args, ending with NULL; to
 * be used with the system call execvp(). Returns a char **, NULL if
 * allocation failed.
 */
static char **extract_argv(char *line) {

    int i, index = 0, nargs = 0;
    size_t nbytes;
    char word[1024];
    char **args = NULL;

    /* Gets the number of arguments in program */
    while ((index = p1getword(line, index, word)) != -1)
        nargs++;

    /* Allocates the array of args */
    nbytes = sizeof(char *) * (nargs + 1);
    if ((args = (char **)malloc(nbytes)) == NULL)
        return NULL;

    index = 0;
    for (i = 0; i < nargs; i++) {
        /* Extract each argument, insert into array */
        index = p1getword(line, index, word);
        args[i] = p1strdup(word);
    }
    /* End array with NULL, needed for execvp() */
    args[nargs] = NULL;

    return args;
}

Process *malloc_pr(char *prog) {

    /* Allocates the memory for process struct */
    Process *pr = (Process *)malloc(sizeof(Process));
    if (pr != NULL) {

        /* Obtains char array of program arguments */
        char **args = extract_argv(prog);
        if (args != NULL) {
            /* Initialzie rest of members */
            pr->argv = args;
            pr->pid = 0;
            pr->ticks = pr->nticks = 0;
            pr->status = WAITING;
            pr->prev_jfs = pr->curr_jfs = 0L;
            pr->prev_util = pr->curr_util = 0L;
        } else {
            /* Allocation failed, free the struct */
            free(pr);
            pr = NULL;
        }
    }

    return pr;
}

char **pr_argv(Process *pr) {

    return pr->argv;
}

void assign_pid(Process *pr, pid_t pid) {

    pr->pid = pid;
}

void assign_ticks(Process *pr, int nticks) {

    pr->ticks = pr->nticks = nticks;
}

int decr_tick(Process *pr) {

    int left = --(pr->ticks);
    /* Reset quantum ticks as needed */
    if (pr->ticks <= 0)
        pr->ticks = pr->nticks;

    return left;
}

pid_t pr_pid(Process *pr) {

    return pr->pid;
}

int pr_status(Process *pr) {

    return pr->status;
}

void pr_wake(Process *pr) {

    pr->status = ALIVE;
}

void pr_kill(Process *pr) {

    pr->status = DEAD;
}

void pr_poll_jiffies(Process *pr, unsigned long jiffies) {

    unsigned long temp = pr->curr_jfs;
    pr->prev_jfs = temp;
    pr->curr_jfs = jiffies;
}

void pr_poll_util(Process *pr, unsigned long util) {

    unsigned long temp = pr->curr_util;
    pr->prev_util = temp;
    pr->curr_util = util;
}

int pr_cpu(Process *pr) {

    int res = 100 * (((float)pr->curr_util - pr->prev_util) /
                ((float)pr->curr_jfs - pr->prev_jfs));
    
    return (res > 100) ? 100 : res;
}

void free_pr(Process *pr) {

    int i;

    if (pr != NULL) {
        /* Need to free each string within array */
        for (i = 0; pr->argv[i] != NULL; i++)
            free(pr->argv[i]);
        /* Free the rest of the members */
        free(pr->argv);
        free(pr);
    }
}

