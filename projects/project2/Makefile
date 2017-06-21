#
# CIS 415 Project 2 Makefile
# Author: Cole Vikupitz
#

CC=gcc
CFLAGS=-W -Wall -g -std=c89
OBJECTS=BoundedBuffer.o diagnostics.o fakeapplications.o \
        freepacketdescriptorstore.o generic_queue.o \
        networkdevice.o networkdriver.o packetdescriptor.o \
        packetdescriptorcreator.o testharness.o

# Builds the driver demo
mydemo: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o mydemo -lpthread

# Cleans project files
clean:
	rm -f networkdriver.o mydemo

# Object files
networkdriver.o: networkdriver.c

