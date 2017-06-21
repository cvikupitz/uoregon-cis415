/*
 * date.c
 * Author: Cole Vikupitz (cvikupit)
 * CIS 415 - Project 0
 *
 * Contains the implementation for an abstract data type for a date object
 * given the header file date.h.
 *
 * This is my own work.
 */

#include <stdio.h>      /* Used for sscanf() */
#include <stdlib.h>     /* Used for malloc(), free(), NULL */
#include "date.h"       /* Date ADT */


/*
 * Struct that represents the Date ADT itself.
 */
struct date {
    
    unsigned short day, month, year;
};


/*
 * date_create creates a Date structure from `datestr`
 * `datestr' is expected to be of the form "dd/mm/yyyy"
 * returns pointer to Date structure if successful,
 *         NULL if not (syntax error)
 */
Date *date_create(char *datestr) {

    Date *date;
    unsigned short day, month, year;

    /* Extract the date information from datestr */
    if (sscanf(datestr, "%hu/%hu/%hu", &day, &month, &year) != 3)
        return NULL;

    /* Make sure the date information given is valid */
    if (day < 1 || day > 31)
        return NULL;
    if (month < 1 || month > 12)
        return NULL;
    if (year < 1 || year > 9999)
        return NULL;

    /* Create the date instance, store the extracted date information */
    if ((date = (Date *)malloc(sizeof(Date))) != NULL) {
        date->day = day;
        date->month = month;
        date->year = year;
    }

    return date;
}

/*
 * date_duplicate creates a duplicate of `d'
 * returns pointer to new Date structure if successful,
 *         NULL if not (memory allocation failure)
 */
Date *date_duplicate(Date *d) {

    Date *clone;

    /* User may not pass in a NULL pointer */
    if (d == NULL)
        return NULL;

    /* Duplicate the date instance, returns the new pointer */
    if ((clone = (Date *)malloc(sizeof(Date))) != NULL)
        *clone = *d;

    return clone;
}

/*
 * date_compare compares two dates, returning <0, 0, >0 if
 * date1<date2, date1==date2, date1>date2, respectively
 */
int date_compare(Date *date1, Date *date2) {

    /* Years are unequal, return the comparison */
    if (date1->year != date2->year)
        return (date1->year - date2->year);

    /* Months are unequal, return the comparison */
    if (date1->month != date2->month)
        return (date1->month - date2->month);

    /* Return comparison between the days */
    return (date1->day - date2->day);
}

/*
 * date_destroy returns any storage associated with `d' to the system
 */
void date_destroy(Date *d) {

    if (d != NULL)
        free(d);
}

