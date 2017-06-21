/*
 * clist.h
 * Author: Cole Vikupitz
 * CIS 415 - Project 1
 *
 * Header file for the CList ADT implementation.
 *
 * This is my own work.
 */

#ifndef _CLIST_H__
#define _CLIST_H__


/*
 * Data structure for a generic singly-linked circular list.
 */
typedef struct clist CList;


/*
 * Creates a new instance of the circular list. Returns pointer to new
 * instance, or NULL if allocation failed.
 */
CList *cl_create(void);

/*
 * Inserts 'item' into the back of the list.
 *
 * Returns 1 if insertion successful, 0 if not (allocation failed).
 */
int cl_insert(CList *cl, void *item);

/*
 * Retrieves the head item in the list, stores result into '*head'.
 *
 * Returns 1 if retrieval successful, 0 if not (list is empty).
 */
int cl_head(CList *cl, void **head);

/*
 * Rotates the linkage of the list, such that, the first element becomes
 * the last, the second becomes the first, third becomes second, etc.
 * Does nothing if empty.
 */
void cl_rotate(CList *cl);

/*
 * Removes the list's first item, stores result into '*head'.
 *
 * Returns 1 if removal was successful, 0 if not (list is empty).
 */
int cl_remove(CList *cl, void **head);

/*
 * Returns the list's current size.
 */
long cl_size(CList *cl);

/*
 * Returns 1/0 indicating whether the list is currently empty.
 */
int cl_isEmpty(CList *cl);

/*
 * Allocates and returns array of all elements in the list, ordered from
 * the head to the tail. Returns NULL if allocation failed or the list
 * is currently empty.
 */
void **cl_toArray(CList *cl, long *len);

/*
 * Destroys the list instance. If destructor != NULL, it is invoked on
 * each item in the list before destruction.
 */
void cl_destroy(CList *cl, void (*destructor)(void *));


#endif/* _CLIST_H__ */

