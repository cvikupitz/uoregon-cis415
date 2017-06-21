#
# CIS 415 Project 0 Makefile
# Author: Cole Vikupitz
#

CC=gcc
CFLAGS=-W -Wall -g
OBJECTS=date.o tldlist.o tldmonitor.o
TEST=date.o tldlistLL.o tldmonitor.o
EXECS=tldmonitor tldmonitorLL

# Builds tldmonitor, AVL version
tldmonitor: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o tldmonitor

# Builds tldmonitor, LinkedList version
tldmonitorLL: $(TEST)
	$(CC) $(CFLAGS) $(TEST) -o tldmonitorLL

# Cleans up project files
clean:
	rm -f $(OBJECTS) $(EXECS)

# Object files
date.o: date.c date.h
tldlist.o: tldlist.c tldlist.h date.h
tldmonitor.o: tldmonitor.c tldlist.h date.h

