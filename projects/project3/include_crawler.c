/*
 * include_crawler.c
 * Author: Cole Vikupitz (cvikupit)
 * CIS 415 - Project 3 (Extra Credit)
 *
 * A file crawler that scans each source file passed in for any non-system
 * directives, and reports them to standard output in the format of a Makefile.
 * Non-system directives are include files that appear as #include "file.h". The
 * program will look for the specified files in any directory specified as an
 * argument, or in the environment variable 'CPATH'. Also supports multi-threading,
 * and will use as many threads specified in the environment variable 'CRAWLER_THREADS'.
 *
 * All work is original, with the exception of the provided python version 'include_crawler.py',
 * which has been converted to C. I have also used the LinkedList and HashMap ADTs from
 * Sventek's GitHub repository provided.
 *
 * UPDATE: The program has now been perfected with the use of the WorkQueue and AnotherStruct
 * ADTs provided in the annotated solution.
 */

#include <stdio.h>              /* Used for stdout, stderr, fprintf(), ... */
#include <stdlib.h>             /* Used for NULL, atoi(), free(), exit(), ... */
#include <string.h>             /* Used for strrchr(), strcpy(), strdup(), strncmp(), ... */
#include <ctype.h>              /* Used for isspace() */
#include <pthread.h>            /* Used for pthread_t, pthread_create(), pthread_join(), ... */
#include "linkedlist.h"         /* LinkedList ADT */
#include "hashmap.h"            /* HashMap ADT */
#include "workqueue.h"          /* WorkQueue ADT */
#include "anotherstruct.h"      /* AnotherStruct ADT */

/* Used to suppress compiler warnings */
#define UNUSED __attribute__((unused))
/* Max size of a reading/writing buffer */
#define SIZE 4096
/* Max number of crawler threads allowed */
#define MAX_THREADS 100


/* Array for directories to browse */
static char **dirs = NULL;

/* WorkQueue ADT to hold the files to process */
static WorkQueue *work_queue = NULL;

/* Map to represent the dependency table */
static AnotherStruct *the_table = NULL;


/* Static method signatures */
/* Extracts and populates dirs array with directories to search through */
static int extract_dirs(int argc, char *argv[]);
/* Populates work queue with files to scan through */
static void prepare_work_queue(int fstart, int argc, char *argv[]);
/* Opens and returns file descriptor for the given file */
static FILE *open_file(const char *afile);
/* Parses the file into its name and extension */
static void parse_file(const char *afile, char *root, char *ext);
/* Threaded function; performs scan on the work queue of files */
static void *process_work_queue(void *args);
/* Opens and reads file, gathers all non-directive dependencies */
static void process_file(const char *afile, LinkedList *deps);
/* Prints out each file's dependencies */
static void print_dependencies(HashMap *printed, LinkedList *to_process);
/* Iterates through the args; prints resulting dependencies */
static void process_results(int fstart, int argc, char *argv[]);
/* Frees all the program's global reserved memory */
static void free_mem(void);
/* Frees the specified linkedlist */
static void free_ll(LinkedList *ll);
/* Prints the specified error message and exits program */
static void print_error(const char *msg);


/*
 * Runs the program.
 */
int main(int argc, char *argv[]) {

    int i, ndirs, fstart;
    int workers = 2;
    size_t nbytes;
    char *crawlers;
    char *cpath, *token;
    char buffer[4096];
    pthread_t thread_pool[MAX_THREADS];

    /* Incorrect usage, print error and exit */
    if (argc < 2) {
        sprintf(buffer, "Usage: %s [-I<directory>]... file.ext...", argv[0]);
        print_error(buffer);
    }

    /* Extract number of worker threads from env variable */
    if ((crawlers = getenv("CRAWLER_THREADS")) != NULL) {
        workers = atoi(crawlers);
        /* Must have at least 1 specified */
        workers = ((workers < 1) ? 1 : workers);
        /* Cannot exceed max threads allowed */
        workers = ((workers > MAX_THREADS) ? MAX_THREADS : workers);
    }

    /* Counts total number of dirs to scan */
    /* Count number of dirs specified with -I flags */
    ndirs = 1;
    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-I", 2))
            break;
        ndirs++;
    }
    /* Count number of dirs in CPATH env variable */
    if ((cpath = getenv("CPATH")) != NULL) {
        token = strtok(cpath, ":");
        while (token != NULL) {
            ndirs++;
            token = strtok(NULL, ":");
        }
    }

    /* Allocates memory for the directory array */
    nbytes = sizeof(char *) * (ndirs + 1);
    dirs = (char **)malloc(nbytes);
    if (dirs == NULL)
        print_error("ERROR: Failed to allocate sufficient amount of memory.");
    /* Populates the array with directories */
    fstart = extract_dirs(argc, argv);

    /* Create the work queue and the dependency table */
    if ((work_queue = wq_create(workers)) == NULL)
        print_error("ERROR: Failed to allocate sufficient amount of memory.");
    if ((the_table = as_create()) == NULL)
        print_error("ERROR: Failed to allocate sufficient amount of memory.");

    /* Adds each file argument into dependency table and work queue */
    prepare_work_queue(fstart, argc, argv);

    /* Process each file in the work queue, start each worker thread */
    for (i = 0; i < workers; i++)
        pthread_create(&(thread_pool[i]), NULL, process_work_queue, NULL);
    for (i = 0; i < workers; i++)
        pthread_join(thread_pool[i], NULL);

    /* Process and print the files inside the dependency table */
    /* Free the program's global reserved memory */
    process_results(fstart, argc, argv);
    free_mem();

    return 0;
}

/*
 * Prepares the list of directories by extracting each directory from the
 * -I flags specified by the user, as well as each specified in the CPATH
 * environment variable. Returns the index in argv[] where the -I flag
 * arguments stop.
 */
static int extract_dirs(int argc, char *argv[]) {

    int i, fstart = 0;
    char *token, *cpath;
    char buffer[SIZE];

    /* Add current dir to array, always checks here first */
    dirs[0] = strdup("./");
    /* Populates array with each dir specified with -I flag */
    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-I", 2)) {
            fstart = i;
            break;
        }
        /* Append '/' if needed */
        strcpy(buffer, argv[i] + 2);
        if (buffer[strlen(buffer) - 1] != '/')
            strcat(buffer, "/");
        dirs[i] = strdup(buffer);
    }

    /* Adds each dir in CPATH env var into dirs array */
    if ((cpath = getenv("CPATH")) != NULL) {
        token = strtok(cpath, ":");
        while (token != NULL) {
            /* Append '/' if needed */
            strcpy(buffer, token);
            if (buffer[strlen(buffer) - 1] != '/')
                strcat(buffer, "/");
            dirs[i++] = strdup(buffer);
            token = strtok(NULL, ":");
        }
    }
    /* End array with NULL */
    dirs[i] = NULL;

    return fstart;
}

/*
 * Processes each argument specified by user after the last -I flag. Adds
 * each file into the workqueue, and adds the object and source file into
 * the dependency table.
 */
static void prepare_work_queue(int fstart, int argc, char *argv[]) {

    LinkedList *obj_deps, *deps;
    char root[256], ext[256], buffer[SIZE];

    for (; fstart < argc; fstart++) {
        /* Extract root name and extension */
        parse_file(argv[fstart], root, ext);
        /* Ensure the file extension is .c, .l, or .y */
        if (strcmp(ext, "c") != 0 &&
            strcmp(ext, "l") != 0 &&
            strcmp(ext, "y") != 0) {
            sprintf(buffer, "Illegal argument: %s must end in .c, .l, or .y", argv[fstart]);
            print_error(buffer);
        }

        /* Create object file, add it and its dependency list into table */
        sprintf(buffer, "%s.o", root);
        if ((obj_deps = ll_create()) == NULL)
            print_error("ERROR: Failed to allocate sufficient amount of memory.");
        if (!ll_add(obj_deps, strdup(argv[fstart])))
            print_error("ERROR: Failed to allocate sufficient amount of memory.");
        if (!as_put(the_table, buffer, obj_deps, NULL))
            print_error("ERROR: Failed to allocate sufficient amount of memory.");
        /* Add the source file and its dependency list into table */
        if (!wq_enqueue(work_queue, strdup(argv[fstart])))
            print_error("ERROR: Failed to allocate sufficient amount of memory.");
        if ((deps = ll_create()) == NULL)
            print_error("ERROR: Failed to allocate sufficient amount of memory.");
        if (!as_put(the_table, argv[fstart], deps, NULL))
            print_error("ERROR: Failed to allocate sufficient amount of memory.");
    }
}

/*
 * Attempts to open the specified file by searching in each directory
 * stored in the directory list. The first directory the file is successfully
 * opened in is used and a pointer to the file descriptor is returned, or
 * NULL if the file could not be opened in any of the directories.
 */
static FILE *open_file(const char *afile) {

    FILE *fd;
    char buffer[SIZE];
    int i;

    for (i = 0; dirs[i] != NULL; i++) {
        /* Retrieves the next directory to browse in */
        sprintf(buffer, "%s%s", dirs[i], afile);
        /* File was opened successfully, return pointer */
        if ((fd = fopen(buffer, "r")) != NULL)
            return fd;
    }

    /* File not found in any directory, print message and return NULL */
    fprintf(stderr, "Unable to open file: %s\n", afile);
    return NULL;
}

/*
 * Parses the file 'afile', then stores the file root name into 'root', and
 * its extension into 'ext'. If the file has no extension, ext will be '',
 * and the root will contain the entire file name.
 *
 * Example: file.foo -> root='file' & ext='foo'
 */
static void parse_file(const char *afile, char *root, char *ext) {

    char *str;
    int i;

    if ((str = strrchr(afile, '.')) == NULL) {
        /* No extension provided, ext will be empty */
        strcpy(root, afile);
        strcpy(ext, "");
    } else {
        /* Perform appropriate string copying calls */
        strcpy(ext, str + 1);
        i = str - afile;
        strncpy(root, afile, i);
        root[i] = '\0';
    }
}

/*
 * Opens the specified file and reads each line, adding each non-system
 * directive into the dependency list 'deps'. If the file is not in the
 * dependency table, it is added as well as into the work queue for
 * processing.
 */
static void process_file(const char *afile, LinkedList *deps) {

    LinkedList *ll;
    FILE *fd;
    int i, start;
    char line[SIZE], fn[SIZE];

    /* Open the file, return if unsuccessful */
    if ((fd = open_file(afile)) == NULL)
        return;

    while (fgets(line, sizeof(line), fd)) {

        i = 0;
        /* Skip all leading white space */
        while (isspace(line[i])) i++;
        /* Ensure line starts with include directive */
        if (strncmp(line + i, "#include", 8)) continue;
        i += 8;
        /* Skip all in-between white space */
        while (isspace(line[i])) i++;
        /* Ensure the directive is non-system */
        if (line[i++] != '"') continue;
        start = i;
        while (line[i] != '\0') {
            if (line[i] == '"') break;
            i++;
        }

        /* Copy file name into buffer, add to dependency list */
        strncpy(fn, line + start, (i - start));
        fn[i - start] = '\0';
        if (!ll_add(deps, strdup(fn)))
            print_error("ERROR: Failed to allocate sufficient amount of memory.");

        if ((ll = ll_create()) == NULL)
            print_error("ERROR: Failed to allocate sufficient amount of memory.");
        /* Adds the file into the dependency list if it doesn't exist */
        if (!as_put_unique(the_table, fn, ll)) {
            ll_destroy(ll, NULL);
        } else {
            if (!wq_enqueue(work_queue, strdup(fn)))
                print_error("ERROR: Failed to allocate sufficient amount of memory.");
        }
    }

    /* File not needed anymore, close it */
    fclose(fd);
}

/*
 * Scans each file in the work queue for its dependencies by invoking
 * process_file(), and updates its corresponding dependency list.
 */
static void *process_work_queue(UNUSED void *args) {

    LinkedList *deps;
    char *afile;

    while (wq_dequeue(work_queue, (void **)&afile)) {
        /* Dequeue the next file for processing, obtain its dependency list from table */
        (void)as_get(the_table, afile, (void **)&deps);
        /* Process the file, free the reserved string */
        process_file(afile, deps);
        free(afile);
    }

    return NULL;
}


/*
 * Iteratively prints out the file dependencies. Uses the specified HashMap
 * 'printed' to keep track of what files are already printed, and to_process
 * to guarantee breadth-first order.
 */
static void print_dependencies(HashMap *printed, LinkedList *to_process) {

    LinkedList *ll;
    long i;
    char *fn, *name;

    while (!ll_isEmpty(to_process)) {
        /* Remove the next file from the processing queue */
        (void)ll_removeFirst(to_process, (void **)&fn);
        (void)as_get(the_table, fn, (void **)&ll);

        for (i = 0L; i < ll_size(ll); i++) {
            /* For each file in dependency list, print if not printed already */
            (void)ll_get(ll, i, (void **)&name);
            if (!hm_containsKey(printed, name)) {
                fprintf(stdout, " %s", name);
                /* Adds file to printed list, and processing queue */
                if (!hm_put(printed, name, NULL, NULL))
                    print_error("ERROR: Failed to allocate sufficient amount of memory.");
                if (!ll_add(to_process, strdup(name)))
                    print_error("ERROR: Failed to allocate sufficient amount of memory.");
            }
        }
        /* Frees the reserved string name in processing queue */
        free(fn);
    }
}

/*
 * Invoked after the work queue is fully processed. Each object file is
 * printed and each source file in its dependency list in the table is also
 * printed.
 */
static void process_results(int fstart, int argc, char *argv[]) {

    HashMap *printed;
    LinkedList *to_process;
    char root[256], ext[256], buffer[SIZE];

    for (; fstart < argc; fstart++) {
        /* For each file arg, print object file */
        parse_file(argv[fstart], root, ext);
        sprintf(buffer, "%s.o", root);
        fprintf(stdout, "%s:", buffer);

        /* Create a processing queue, and a list of what's been printed, add the file to queue */
        if ((printed = hm_create(0L, 0.0f)) == NULL)
            print_error("ERROR: Failed to allocate sufficient amount of memory.");
        if ((to_process = ll_create()) == NULL)
            print_error("ERROR: Failed to allocate sufficient amount of memory.");
        if (!ll_add(to_process, strdup(buffer)))
            print_error("ERROR: Failed to allocate sufficient amount of memory.");

        /* Print out each file's dependecies */
        print_dependencies(printed, to_process);
        puts("");
        /* Destroy the processing and printed ADTs */
        hm_destroy(printed, NULL);
        ll_destroy(to_process, free);
    }
}

/*
 * Frees all program's reserved global memory back to the heap.
 */
static void free_mem(void) {

    int i;

    /* Free the array of directories */
    if (dirs != NULL) {
        for (i = 0; dirs[i] != NULL; i++)
            free(dirs[i]);
        free(dirs);
    }
    /* Free the work queue of files */
    if (work_queue != NULL)
        wq_destroy(work_queue, free);
    /* Destroy the dependency table with free_ll */
    if (the_table != NULL)
        as_destroy(the_table, (void *)free_ll);
}

/*
 * Frees the specified linked list by invoking the destructor with free(),
 * used for destroying linked lists inside a hashmap.
 */
static void free_ll(LinkedList *ll) {

    if (ll != NULL)
        ll_destroy(ll, free);
}

/*
 * Prints the specified error message and exits the program.
 */
static void print_error(const char *msg) {

    fprintf(stderr, "%s\n", msg);
    /* Also need to free global memory back to heap */
    free_mem();
    exit(-1);
}
