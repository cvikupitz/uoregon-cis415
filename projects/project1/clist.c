/*
 * clist.c
 * Author: Cole Vikupitz
 * CIS 415 - Project 1
 *
 * Source file for the CList ADT implementation. Contains an implementation
 * of a generic circular singly-linked list.
 *
 * This is my own work.
 */

#include <stdlib.h>     /* Used for malloc(), free() */
#include "clist.h"      /* CList ADT */


/*
 * Node struct stored inside the list
 */
typedef struct cnode {
    void *item;                 /* The stored item */
    struct cnode *next;         /* Pointer to next node in list */
} CNode;

/*
 * Struct that represents the list itself
 */
struct clist {
    CNode *tail;        /* Pointer to tail node */
    long size;          /* Size of the list */
};


CList *cl_create(void) {

    /* Allocate memory, initialize the members */
    CList *cl = (CList *)malloc(sizeof(CList));
    if (cl != NULL) {
        cl->tail = NULL;
        cl->size = 0L;
    }

    return cl;
}

int cl_insert(CList *cl, void *item) {

    /* Creates the new node to insert */
    CNode *node = (CNode *)malloc(sizeof(CNode));
    if (node == NULL)
        return 0;

    if (cl->size == 0L) {
        /* Empty list, point node to itself */
        cl->tail = node;
        node->next = node;
    } else {
        /* Otherwise, relink as needed */
        node->next = cl->tail->next;
        cl->tail->next = node;
    }

    /* Perform other updattes on list */
    node->item = item;
    cl->tail = cl->tail->next;
    cl->size++;

    return 1;
}

int cl_head(CList *cl, void **head) {

    if (cl->size == 0L)
        return 0;
    *head = cl->tail->next->item;

    return 1;
}

void cl_rotate(CList *cl) {

    if (cl->size != 0L)/* Only rotate if not empty */
        cl->tail = cl->tail->next;
}

int cl_remove(CList *cl, void **head) {

    if (cl->size == 0L)
        return 0;

    /* Extracts the head item */
    CNode *node = cl->tail->next;
    *head = node->item;

    /* Perform relinking, depending on the list's size */
    if (cl->size == 1L)
        cl->tail = NULL;
    else
        cl->tail->next = node->next;

    free(node);
    cl->size--;

    return 1;
}

long cl_size(CList *cl) {

    return cl->size;
}

int cl_isEmpty(CList *cl) {

    return (cl->size == 0L);
}

void **cl_toArray(CList *cl, long *len) {

    CNode *temp;
    void **array;
    long i;

    /* List is empty, do nothing */
    if (cl->size == 0L)
        return NULL;

    *len = cl->size;
    if ((array = (void **)malloc(sizeof(void *) * cl->size)) != NULL) {
        /* Traverse each node startig with head */
        /* Populate array with items in each node */
        temp = cl->tail->next;
        for (i = 0L; i < cl->size; i++) {
            array[i] = temp->item;
            temp = temp->next;
        }
    }

    return array;
}

void cl_destroy(CList *cl, void (*destructor)(void *)) {

    void *item;

    if (cl != NULL) {
        while (cl_remove(cl, &item))
            /* Invoke destructor on item if not NULL */
            if (destructor != NULL)
                (*destructor)(item);
        free(cl);
    }
}

