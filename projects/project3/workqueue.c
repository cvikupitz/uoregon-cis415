/*
 * workqueue.c
 * Author: Cole Vikupitz
 * CIS 415 - Project 3 (Extra Credit)
 *
 * Source code for a generic thread-safe work queue ADT used in the workflow diagram.
 */

#include <stdlib.h>             /* Used for malloc(), free() */
#include <pthread.h>            /* Used for pthread_mutex methods */
#include "workqueue.h"          /* WorkQueue ADT */
#include "linkedlist.h"         /* LinkedList ADT */


/*
 * Struct that represents the work queue
 */
struct workqueue {
    pthread_mutex_t mutex;      /* The lock */
    pthread_cond_t condition;   /* Condition to wait */
    LinkedList *queue;          /* Inner linkedlist used as queue */
    int workers;                /* Number of worker threads */
    long size;                  /* Size of the work queue */
};


WorkQueue *wq_create(int nworkers) {

    WorkQueue *wq;
    LinkedList *ll;

    /* Allocate the memory for workqueue struct */
    if ((wq = (WorkQueue *)malloc(sizeof(WorkQueue))) != NULL) {
        if ((ll = ll_create()) == NULL) {
            /* Failed to allocate, return NULL */
            free(wq);
            wq = NULL;
        } else {
            /* Initiailize struct members */
            pthread_mutex_init(&(wq->mutex), NULL);
            pthread_cond_init(&(wq->condition), NULL);
            wq->queue = ll;
            wq->workers = nworkers;
            wq->size = 0L;
        }
    }

    return wq;
}

int wq_enqueue(WorkQueue *wq, void *element) {

    int res;

    /* Acquire lock, enqueue the item */
    pthread_mutex_lock(&(wq->mutex));
    res = ll_add(wq->queue, element);
    wq->size++;
    /* Broadcast, release lock */
    pthread_cond_broadcast(&(wq->condition));
    pthread_mutex_unlock(&(wq->mutex));

    return res;
}

int wq_dequeue(WorkQueue *wq, void **element) {

    int res = 0;

    /* Acquire the lock to dequeue item */
    pthread_mutex_lock(&(wq->mutex));
    wq->workers--;
    while (wq->size == 0L && wq->workers > 0)
        pthread_cond_wait(&(wq->condition), &(wq->mutex));

    /* Dequeue the item, update queue members */
    if (wq->size > 0L) {
        res = ll_removeFirst(wq->queue, element);
        wq->size--;
        wq->workers++;
    }

    /* Broadcast, release the lock */
    pthread_cond_broadcast(&(wq->condition));
    pthread_mutex_unlock(&(wq->mutex));

    return res;
}

void wq_destroy(WorkQueue *wq, void (*userFunction)(void *element)) {

    if (wq != NULL) {
        /* Acquire the lock for destruction */
        pthread_mutex_lock(&(wq->mutex));
        /* Destroy inner linkedlist */
        if (wq->queue != NULL)
            ll_destroy(wq->queue, userFunction);
        /* Destroy other members */
        pthread_mutex_unlock(&(wq->mutex));
        pthread_mutex_destroy(&(wq->mutex));
        pthread_cond_destroy(&(wq->condition));
        free(wq);
    }
}

