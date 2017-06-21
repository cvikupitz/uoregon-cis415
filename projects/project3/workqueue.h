/*
 * workqueue.h
 * Author: Cole Vikupitz
 * CIS 415 - Project 3 (Extra Credit)
 *
 * Header file for a generic thread-safe work queue ADT used in the workflow diagram.
 */

#ifndef __WORKQUEUE_H_
#define __WORKQUEUE_H_


/*
 * An ADT data type to represent the 'Work Queue' in the worker flow diagram.
 */
typedef struct workqueue WorkQueue;


/*
 * Creates an instance of the WorkQueue ADT and returns a pointer, or NULL if
 * memory allocation failed. User passes in the number of threads that will be
 * working on the work queue.
 */
WorkQueue *wq_create(int workers);

/*
 * Adds the specified element 'element' to the work queue. Returns 1 if addition
 * is successful, or 0 if not (e.g. malloc() error).
 */
int wq_enqueue(WorkQueue *wq, void *element);

/*
 * Dequeues the front element in the queue, stores result into '*element'. Returns
 * 1 iff removal was successful, 0 if not (queue is empty).
 */
int wq_dequeue(WorkQueue *wq, void **element);

/*
 * Destroys the specified WorkQueue instance by freeing its reserved memory back
 * to the heap.
 */
void wq_destroy(WorkQueue *wq, void (*userFunction)(void *element));


#endif/* __WORKQUEUE_H_ */

