/*
 * tldlist.c
 * Author: Cole Vikupitz (cvikupit)
 * CIS 415 - Project 0
 *
 * Contains the implementation for the abstract data types for a TLDList organized
 * as an AVL tree, TLDNodes stored inside the TLDList, and an iterator for the
 * TLDList. Implemented given the header file tldlist.h.
 *
 * Note the following functions listed below are not my original work. Code
 * is borrowed from Mark Allen Weiss's Java implementation of an AVL tree, which
 * can be found here:
 * https://users.cs.fiu.edu/~weiss/dsaajava/code/DataStructures/AvlTree.java
 *
 * - rotateWithLeftChild()
 * - rotateWithRightChild()
 * - doubleWithLeftChild()
 * - doubleWithRightChild()
 * - tldlist_insert()
 */

#include <stdlib.h>     /* Used for malloc(), free(), NULL */
#include <string.h>     /* Used for strlen(), strcmp() */
#include <ctype.h>      /* Used for tolower() */
#include "tldlist.h"    /* TLDList ADT */
#include "date.h"       /* Date ADT */


/*
 * Struct that represents the TLDList itself.
 */
struct tldlist {
    TLDNode *root;              /* Pointer to root */
    Date *begin, *end;          /* Date ADTs signifying the date range */
    long size, count;           /* Size ann number of entries in list */
};

/*
 * Struct that represents a node in the TLDList.
 */
struct tldnode {
    TLDNode *left, *right;      /* Pointers to left and right child nodes */
    char *tld;                  /* The stored tld */
    int height;                 /* Height in the tree, used for rebalancing */
    long count;                 /* Number of log entries for this tld */
};

/*
 * Struct that represents the TLDIterator.
 */
struct tlditerator {
    TLDNode **elements;         /* The array of items to iterate */
    long next, size;            /* Index of next item, and size of array */
};


/*
 * tldlist_create generates a list structure for storing counts against
 * top level domains (TLDs)
 *
 * creates a TLDList that is constrained to the `begin' and `end' Date's
 * returns a pointer to the list if successful, NULL if not
 */
TLDList *tldlist_create(Date *begin, Date *end) {

    Date *begin_dup, *end_dup;
    TLDList *new_tld;

    /* Return NULL if user passes in any NULL pointers, or the date range is invalid */
    if (begin == NULL || end == NULL || date_compare(begin, end) > 0)
        return NULL;

    /* Duplicate the dates for the struct itself, and allocate space for new TLDList */
    /* Deallocate and return NULL if failed */
    if ((begin_dup = date_duplicate(begin)) == NULL ||
        (end_dup = date_duplicate(end)) == NULL ||
        (new_tld = (TLDList *)malloc(sizeof(TLDList))) == NULL) {
        
        tldlist_destroy(new_tld);
        date_destroy(begin_dup);
        date_destroy(end_dup);
        return NULL;
    }

    /* Initialize the instance members */
    new_tld->begin = begin_dup;
    new_tld->end = end_dup;
    new_tld->root = NULL;
    new_tld->count = new_tld->size = 0L;

    return new_tld;
}

/*
 * Extracts the top-level domain from 'hostname' and stores the result
 * into 'dest'. Also converts the tld into lowercase.
 */
static void hostname_to_tld(char *hostname, char *dest) {

    char *res;
    int i;

    /* Extracts the tld, stores into dest buffer */
    res = strrchr(hostname, '.');
    if (res == NULL)
        strcpy(dest, hostname);
    else
        strcpy(dest, ++res);

    /* Converts the tld to lowercase */
    for (i = 0; dest[i]; i++)
        dest[i] = tolower(dest[i]);
}

/*
 * Creates and returns a new TLDNode to be stored into the TLDList, given
 * the tld it will store. Returns pointer to new instance, NULL if allocation
 * failed.
 */
static TLDNode *tldnode_create(char *tld) {

    TLDNode *new_node;

    /* Allocate memory for the TLDNode, return NULL if failed */
    if ((new_node = (TLDNode *)malloc(sizeof(TLDNode))) != NULL) {

        /* Allocate memory for the tld, return NULL if failed */
        new_node->tld = (char *)malloc(strlen(tld) + 1);
        if (new_node->tld == NULL) {
            free(new_node);
            return NULL;
        }

        /* Initialize the members */
        new_node->count = 1L;
        new_node->height = 0;
        new_node->left = new_node->right = NULL;
        strcpy(new_node->tld, tld);
    }

    return new_node;
}

/*
 * Destroys all the TLDNode instances in the TLDList; performed by doing
 * a post-order traversal.
 */
static void destroy_nodes(TLDNode *node) {

    /* Node has no children, return */
    if (node == NULL)
        return;

    /* Traverse all children of node first */
    destroy_nodes(node->left);
    destroy_nodes(node->right);

    /* Free up all reserved memory associated with the node */
    free(node->tld);
    free(node);
}

/*
 * tldlist_destroy destroys the list structure in `tld'
 *
 * all heap allocated storage associated with the list is returned to the heap
 */
void tldlist_destroy(TLDList *tld) {

    if (tld != NULL) {
        /* Each individual node will need to be destroyed */
        destroy_nodes(tld->root);
        /* Destroys the dates */
        date_destroy(tld->begin);
        date_destroy(tld->end);
        /* Free the tldlist itself */
        free(tld);
    }
}

/*
 * Searches the TLDList for the specified tld. Returns pointer to the TLDNode
 * that contains the tld, NULL if the tld wasn't found.
 */
static TLDNode *tldlist_search(char *tld, TLDNode *node) {

    /* Dead end hit, tld is not in the tree */
    if (node == NULL)
        return NULL;

    int cmp = strcmp(tld, node->tld);
    if (cmp < 0)        /* tld is in the left subtree */
        return tldlist_search(tld, node->left);
    else if (cmp > 0)   /* tld is in the right subtree */
        return tldlist_search(tld, node->right);
    else                /* tld was found */
        return node;
}

/*
 * Returns the maximum of the two given numbers. Used for comparing node heights.
 */
static int maximum(int a, int b) {

    return ((a > b) ? a : b);
}

/*
 * Returns the current height of the given TLDNode instance, or -1 if the
 * TLDNode given is a NULL pointer. Used for rebalancing the TLDList.
 */
static int height(TLDNode *node) {

    return ((node != NULL) ? node->height : -1);
}

/*
 * Internal function for a rotation of the given TLDNode for rebalancing.
 *
 * This function originally comes from an AVL implementation written in Java by
 * Mark Allen Weiss. I've then converted the code into C.
 *
 * Implementation may be found here:
 * https://users.cs.fiu.edu/~weiss/dsaajava/code/DataStructures/AvlTree.java
 */
static TLDNode *rotateWithLeftChild(TLDNode *k2) {

    TLDNode *k1 = k2->left;
    k2->left = k1->right;
    k1->right = k2;
    k2->height = maximum(height(k2->left), height(k2->right)) + 1;
    k1->height = maximum(height(k1->left), k2->height) + 1;
    return k1;
}

/*
 * Internal function for a rotation of the given TLDNode for rebalancing.
 *
 * This function originally comes from an AVL implementation written in Java by
 * Mark Allen Weiss. I've then converted the code into C.
 *
 * Implementation may be found here:
 * https://users.cs.fiu.edu/~weiss/dsaajava/code/DataStructures/AvlTree.java
 */
static TLDNode *rotateWithRightChild(TLDNode *k1) {

    TLDNode *k2 = k1->right;
    k1->right = k2->left;
    k2->left = k1;
    k1->height = maximum(height(k1->left), height(k1->right)) + 1;
    k2->height = maximum(height(k2->right), k1->height) + 1;
    return k2;
}

/*
 * Internal function for a double rotation of the given TLDNode for rebalancing.
 *
 * This function originally comes from an AVL implementation written in Java by
 * Mark Allen Weiss. I've then converted the code into C.
 *
 * Implementation may be found here:
 * https://users.cs.fiu.edu/~weiss/dsaajava/code/DataStructures/AvlTree.java
 */
static TLDNode *doubleWithLeftChild(TLDNode *k3) {

    k3->left = rotateWithRightChild(k3->left);
    return rotateWithLeftChild(k3);
}

/*
 * Internal function for a double rotation of the given TLDNode for rebalancing.
 *
 * This function originally comes from an AVL implementation written in Java by
 * Mark Allen Weiss. I've then converted the code into C.
 *
 * Implementation may be found here:
 * https://users.cs.fiu.edu/~weiss/dsaajava/code/DataStructures/AvlTree.java
 */
static TLDNode *doubleWithRightChild(TLDNode *k1) {

    k1->right = rotateWithLeftChild(k1->right);
    return rotateWithRightChild(k1);
}


/*
 * Internal function for inserting nodes into the AVL tree. Will also check for tree
 * balance and calls rebalancing methods if needed.
 *
 * This function originally comes from an AVL implementation written in Java by
 * Mark Allen Weiss. I've then converted the code into C.
 *
 * Implementation may be found here:
 * https://users.cs.fiu.edu/~weiss/dsaajava/code/DataStructures/AvlTree.java
 */
static TLDNode *tldlist_insert(TLDNode *node, TLDNode *other) {

    if (other == NULL) {
        other = node;
    } else if (strcmp(node->tld, other->tld) < 0) {
        
        other->left = tldlist_insert(node, other->left);
        if (height(other->left) - height(other->right) == 2) {
            if (strcmp(node->tld, other->left->tld) < 0)
                other = rotateWithLeftChild(other);
            else
                other = doubleWithLeftChild(other);
        }
    } else if (strcmp(node->tld, other->tld) > 0) {
    
        other->right = tldlist_insert(node, other->right);
        if (height(other->right) - height(other->left) == 2) {
            if (strcmp(node->tld, other->right->tld) > 0)
                other = rotateWithRightChild(other);
            else
                other = doubleWithRightChild(other);
        }
    } else { /* Duplicate entry, do nothing */ }

    other->height = maximum(height(other->left), height(other->right)) + 1;
    return other;
}

/*
 * tldlist_add adds the TLD contained in `hostname' to the tldlist if
 * `d' falls in the begin and end dates associated with the list;
 * returns 1 if the entry was counted, 0 if not
 */
int tldlist_add(TLDList *tld, char *hostname, Date *d) {

    TLDNode *res, *temp;
    char buffer[256];

    /* Return 0 if user passes in any NULL pointers, or the date given is out of range */
    if (tld == NULL || hostname == NULL || d == NULL ||
        date_compare(d, tld->begin) < 0 || date_compare(d, tld->end) > 0)
        return 0;

    /* Gather and store the tld from the given hostname into the buffer */
    hostname_to_tld(hostname, buffer);

    /* Search the tree, see if tld already exists */
    if ((res = tldlist_search(buffer, tld->root)) == NULL) {
        if ((temp = tldnode_create(buffer)) == NULL)
            return 0;
        /* tld not in TLDList, create new node and insert into TLDList */
        tld->root = tldlist_insert(temp, tld->root);
        tld->size++;
    } else {
        /* tld already exists in TLDList, increment its counter */
        res->count++;
    }

    tld->count++;
    return 1;
}

/*
 * tldlist_count returns the number of successful tldlist_add() calls since
 * the creation of the TLDList
 */
long tldlist_count(TLDList *tld) {

    return ((tld != NULL) ? tld->count : 0L);
}

/*
 * Populates the iterator with all the TLDNode instances in the tree; done
 * by performing an in-order traversal.
 */
static void inorder_traversal(TLDIterator *iter, TLDNode *node) {

    if (node == NULL)
        return;
    inorder_traversal(iter, node->left);
    iter->elements[iter->next++] = node;
    inorder_traversal(iter, node->right);
}

/*
 * tldlist_iter_create creates an iterator over the TLDList; returns a pointer
 * to the iterator if successful, NULL if not
 */
TLDIterator *tldlist_iter_create(TLDList *tld) {

    TLDIterator *new_iter;

    /* User may not pass in a NULL pointer */
    if (tld == NULL)
        return NULL;

    /* Allocate the memory for the new instance */
    if ((new_iter = (TLDIterator *)malloc(sizeof(TLDIterator))) != NULL) {
        new_iter->elements = (TLDNode **)malloc(tld->size * sizeof(TLDNode *));

        /* Memory allocation failed, abort the iterator creation */
        if (new_iter->elements == NULL) {
            free(new_iter);
            return NULL;
        }

        /* Initialize the members */
        new_iter->next = 0L;
        new_iter->size = tld->size;
        inorder_traversal(new_iter, tld->root);
        new_iter->next = 0L;
    }

    return new_iter;
}

/*
 * tldlist_iter_next returns the next element in the list; returns a pointer
 * to the TLDNode if successful, NULL if no more elements to return
 */
TLDNode *tldlist_iter_next(TLDIterator *iter) {

    if (iter == NULL)
        return NULL;
    return ((iter->next != iter->size) ? iter->elements[iter->next++] : NULL);
}

/*
 * tldlist_iter_destroy destroys the iterator specified by `iter'
 */
void tldlist_iter_destroy(TLDIterator *iter) {

    if (iter != NULL) {
        free(iter->elements);
        free(iter);
    }
}

/*
 * tldnode_tldname returns the tld associated with the TLDNode
 */
char *tldnode_tldname(TLDNode *node) {

    return ((node != NULL) ? node->tld : NULL);
}

/*
 * tldnode_count returns the number of times that a log entry for the
 * corresponding tld was added to the list
 */
long tldnode_count(TLDNode *node) {

    return ((node != NULL) ? node->count : 0L);
}

