/*
 * anotherstruct.c
 * Author: Cole Vikupitz (cvikupit)
 * CIS 415 - Project 3 (Extra Credit)
 *
 * Source code for the 'AnotherStructure' ADT shown in the worker flow diagram.
 */

#include <stdlib.h>             /* malloc(), free() */
#include <pthread.h>            /* pthread_mutex methods */
#include "anotherstruct.h"      /* AnotherStruct ADT */
#include "hashmap.h"            /* HashMap ADT */


/*
 * Struct that represents anotherstruct in the worker diagram.
 */
struct anotherstruct {
    pthread_mutex_t mutex;      /* The lock */
    HashMap *table;             /* The inner hashtable */
};


AnotherStruct *as_create(void) {

    HashMap *table;
    AnotherStruct *as;

    /* Allocate memory for struct */
    if ((as = (AnotherStruct *)malloc(sizeof(AnotherStruct))) != NULL) {
        if ((table = hm_create(0L, 0.0f)) == NULL) {
            /* Allocation failed, return NULL */
            free(as);
            as = NULL;
        } else {
            /* Initialize other members */
            pthread_mutex_init(&(as->mutex), NULL);
            as->table = table;
        }
    }

    return as;
}

int as_put(AnotherStruct *as, char *key, void *element, void **previous) {

    int res;

    /* Acquire lock, insert item, release lock */
    pthread_mutex_lock(&(as->mutex));
    res = hm_put(as->table, key, element, previous);
    pthread_mutex_unlock(&(as->mutex));

    return res;
}

int as_put_unique(AnotherStruct *as, char *key, void *element) {

    int res = 0;

    /* Acquire lock, insert if not present, release lock */
    pthread_mutex_lock(&(as->mutex));
    if (!hm_containsKey(as->table, key))
        res = hm_put(as->table, key, element, NULL);
    pthread_mutex_unlock(&(as->mutex));

    return res;
}

int as_get(AnotherStruct *as, char *key, void **element) {

    int res;

    /* Acquire lock, get item, release lock */
    pthread_mutex_lock(&(as->mutex));
    res = hm_get(as->table, key, element);
    pthread_mutex_unlock(&(as->mutex));

    return res;
}

void as_destroy(AnotherStruct *as, void (*userFunction)(void *element)) {

    if (as != NULL) {
        /* Acquire lock for destruction */
        pthread_mutex_lock(&(as->mutex));
        /* Destroy inner hashtable */
        if (as->table != NULL)
            hm_destroy(as->table, userFunction);
        /* Destroy rest of members */
        pthread_mutex_unlock(&(as->mutex));
        pthread_mutex_destroy(&as->mutex);
        free(as);
    }
}
