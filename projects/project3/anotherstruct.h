/*
 * anotherstruct.h
 * Author: Cole Vikupitz (cvikupit)
 * CIS 415 - Project 3 (Extra Credit)
 *
 * Header file for the 'AnotherStructure' ADT shown in the worker flow diagram.
 */

#ifndef __ANOTHERSTRUCT_H_
#define __ANOTHERSTRUCT_H_


/*
 * An ADT data type to represent the 'Another Structure' in the worker flow diagram.
 */
typedef struct anotherstruct AnotherStruct;


/*
 * Creates an instance of the AnotherStruct ADT and returns a pointer, or NULL
 * if memory allocation failed.
 */
AnotherStruct *as_create(void);

/*
 * Adds the specified key 'key' and its specified corresponding element 'element'
 * into the table. If the key already exists, its previous element is stored into
 * the address of 'previous'. Returns 1 if added successfully, or 0 if not
 * (e.g. malloc() error).
 */
int as_put(AnotherStruct *as, char *key, void *element, void **previous);

/*
 * Adds the specified key and its maping element to the table, only if it does not
 * currently exist in the table. Returns 1 if successfully added, or 0 if not
 * (key already exists, or malloc() error).
 */
int as_put_unique(AnotherStruct *as, char *key, void *element);

/*
 * Obtains the element that is currently mapped to the specified key and stores the
 * element into '*element'. Returns 1 if the key was found, or 0 if not.
 */
int as_get(AnotherStruct *as, char *key, void **element);

/*
 * Destroys the specified AnotherStruct instance by freeing its reserved memory back
 * to the heap.
 */
void as_destroy(AnotherStruct *as, void (*userFunction)(void *element));


#endif/* __ANOTHERSTRUCT_H_ */

