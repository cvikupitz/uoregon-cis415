/*
 * process.h
 * Author: Cole Vikupitz
 * CIS 415 - Project 1
 *
 * Header file for the process ADT implementation.
 *
 * This is my own work.
 *
 * UPDATE: I have now added quantum ticks into the implementation as shown in
 * the solution.
 */

#ifndef _PROCESS_H__
#define _PROCESS_H__

#include <unistd.h>     /* Used for pid_t type */

/* Macros for process status */
#define WAITING 16      /* Process waiting for execution */
#define ALIVE 17        /* Process has started execution */
#define DEAD 18         /* Process finished with execution */


/*
 * Data structure representing a process to run in a system.
 */
typedef struct process Process;


/*
 * Creates and returns a pointer to a new instance of the process ADT.
 * The line given represents a program invoked on the command line (ex.
 * 'echo this', 'date', 'ls -lh /home/', etc). Returns a pointer to the
 * new instance, NULL if allocation failed.
 */
Process *malloc_pr(char *prog);

/*
 * Returns a char** representation of the process to run. This is the
 * program that is run on the command line, ends the array with NULL.
 * Intended to be used with the system call execvp().
 *
 * ex: 'echo this' -> ['echo', 'this', NULL].
 */
char **pr_argv(Process *pr);

/*
 * Assigns the pid_t 'pid' to the process instance.
 */
void assign_pid(Process *pr, pid_t pid);

/*
 * Assign the number of quantum ticks to the specified process.
 */
void assign_ticks(Process *pr, int nticks);

/*
 * Decrements the number of quantum ticks for the specified process.
 * If the ticks left after decrementing is <= 0, the ticks are reset.
 * Returns the number of ticks process has left after the decrement.
 */
int decr_tick(Process *pr);

/*
 * Returns the pid_t of the specified process.
 */
pid_t pr_pid(Process *pr);

/*
 * Returns the process's current status; either WAITING, ALIVE, or
 * DEAD. Uses the macros defined in process.h
 */
int pr_status(Process *pr);

/*
 * Sets the specified process's status to ALIVE.
 */
void pr_wake(Process *pr);

/*
 * Sets the specified process's status to DEAD.
 */
void pr_kill(Process *pr);

/*
 * Stores the jiffies amount into the process, used for CPU calculation.
 */
void pr_poll_jiffies(Process *pr, unsigned long jiffies);

/*
 * Stores the utilization clock ticks into the process, used for CPU
 * calculation.
 */
void pr_poll_util(Process *pr, unsigned long util);

/*
 * Calculates the CPU utilization for this process.
 */
int pr_cpu(Process *pr);

/*
 * Destroys the specified process instance, frees all reserved memory.
 */
void free_pr(Process *pr);


#endif/* _PROCES_H__ */

