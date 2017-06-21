#
# CIS 415 Project 3 Makefile
# Author: Cole Vikupitz
#

CC=gcc
CFLAGS=-W -Wall -g
HEADERS=linkedlist.h hashmap.h workqueue.h anotherstruct.h
OBJECTS=include_crawler.o linkedlist.o hashmap.o workqueue.o\
	anotherstruct.o
EXEC=include_crawler

# Builds the include crawler executable
include_crawler: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o include_crawler -lpthread

# Cleans project files
clean:
	rm -f $(OBJECTS) $(EXEC)

# Object files
include_crawler.o: include_crawler.c $(HEADERS)
linkedlist.o: linkedlist.c linkedlist.h
hashmap.o: hashmap.c hashmap.h
workqueue.o: workqueue.c workqueue.h linkedlist.h
anotherstruct.o: anotherstruct.c anotherstruct.h hashmap.h

