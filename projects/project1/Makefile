#
# CIS 415 Project 1 Makefile
# Author: Cole Vikupitz
#

CC=gcc
CFLAGS=-W -Wall -g
OBJECTS=clist.o process.o p1fxns.o uspsv1.o uspsv2.o uspsv3.o uspsv4.o uspsv5.o \
        cpubound.o iobound.o
TESTS=cpubound iobound
EXECS=uspsv1 uspsv2 uspsv3 uspsv4 uspsv5

# Builds all versions of USPS
all: $(EXECS)

# Builds testing applications
test: $(TESTS)

# Builds USPS v1
uspsv1: uspsv1.o clist.o process.o p1fxns.o
	$(CC) $(CFLAGS) uspsv1.o clist.o process.o p1fxns.o -o uspsv1

# Builds USPS v2
uspsv2: uspsv2.o clist.o process.o p1fxns.o
	$(CC) $(CFLAGS) uspsv2.o clist.o process.o p1fxns.o -o uspsv2

# Builds USPS v3
uspsv3: uspsv3.o clist.o process.o p1fxns.o
	$(CC) $(CFLAGS) uspsv3.o clist.o process.o p1fxns.o -o uspsv3

# Builds USPS v4
uspsv4: uspsv4.o clist.o process.o p1fxns.o
	$(CC) $(CFLAGS) uspsv4.o clist.o process.o p1fxns.o -o uspsv4

# Builds USPS v5
uspsv5: uspsv5.o clist.o process.o p1fxns.o
	$(CC) $(CFLAGS) uspsv5.o clist.o process.o p1fxns.o -o uspsv5

# Builds CPU-bound test program
cpubound: cpubound.o
	$(CC) $(CFLAGS) cpubound.o -o cpubound

# Builds IO-bound test program
iobound: iobound.o
	$(CC) $(CFLAGS) iobound.o -o iobound

# Cleans up project files
clean:
	rm -f $(OBJECTS) $(EXECS) $(TESTS) 

# Object files
clist.o: clist.c clist.h
cpubound.o: cpubound.c
iobound.o: iobound.c
process.o: process.c process.h p1fxns.h
p1fxns.o: p1fxns.c p1fxns.h
uspsv1.o: uspsv1.c clist.h process.h p1fxns.h
uspsv2.o: uspsv2.c clist.h process.h p1fxns.h
uspsv3.o: uspsv3.c clist.h process.h p1fxns.h
uspsv4.o: uspsv4.c clist.h process.h p1fxns.h
uspsv5.o: uspsv5.c clist.h process.h p1fxns.h

