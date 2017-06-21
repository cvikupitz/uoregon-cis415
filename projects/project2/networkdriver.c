/*
 * networkdriver.c
 * Author: Cole Vikupitz (cvikupit)
 * CIS 415 - Project 2
 *
 * A network device that functions as a (de)multiplexor for a simple network
 * device. Makes calls to pass data packets to the network, and also receives
 * packets and passes them to the waiting callers. Focuses more on concurrency
 * than on netorking issues.
 *
 * This code is my own work, with the exception of using the provided files.
 * The documentation above the functions and some of the pseudocode are also
 * copied from the handout.
 */

#include <pthread.h>
#include "BoundedBuffer.h"
#include "destination.h"
#include "diagnostics.h"
#include "fakeapplications.h"
#include "freepacketdescriptorstore.h"
#include "freepacketdescriptorstore__full.h"
#include "generic_queue.h"
#include "networkdevice.h"
#include "networkdevice__full.h"
#include "networkdriver.h"
#include "packetdescriptor.h"
#include "packetdescriptorcreator.h"
#include "pid.h"

/* Macro used to suppress compiler warnings */
#define UNUSED __attribute__((unused))
/* Size of BB in each incoming BB */
#define IN_BUFFER 4

/* Global pd store and network device used by driver */
static FreePacketDescriptorStore *fpds_t;
static NetworkDevice *nd_t;

/* Bounded buffers used by the driver */
static BoundedBuffer *outgoing;
static BoundedBuffer *incoming[MAX_PID + 1];

/* Function prototypes used by the threads */
static void *sendFxn(void *arg);
static void *recvFxn(void *arg);

/* These calls hand in a PacketDescriptor for dispatching */
/* The nonblocking call must return promptly, indicating whether or */
/* not the indicated packet has been accepted by your code          */
/* (it might not be if your internal buffer is full) 1=OK, 0=not OK */
/* The blocking call will usually return promptly, but there may be */
/* a delay while it waits for space in your buffers.                */
/* Neither call should delay until the packet is actually sent      */
void blocking_send_packet(PacketDescriptor *pd) {

    blockingWriteBB(outgoing, (void *)pd);
}

int nonblocking_send_packet(PacketDescriptor *pd) {

    return nonblockingWriteBB(outgoing, (void *)pd);
}


/* These represent requests for packets by the application threads */
/* The nonblocking call must return promptly, with the result 1 if */
/* a packet was found (and the first argument set accordingly) or  */
/* 0 if no packet was waiting.                                     */
/* The blocking call only returns when a packet has been received  */
/* for the indicated process, and the first arg points at it.      */
/* Both calls indicate their process number and should only be     */
/* given appropriate packets. You may use a small bounded buffer   */
/* to hold packets that haven't yet been collected by a process,   */
/* but are also allowed to discard extra packets if at least one   */
/* is waiting uncollected for the same PID. i.e. applications must */
/* collect their packets reasonably promptly, or risk packet loss. */
void blocking_get_packet(PacketDescriptor **pd, PID pid) {

    *pd = (PacketDescriptor *)blockingReadBB(incoming[pid]);
}

int nonblocking_get_packet(PacketDescriptor **pd, PID pid) {

    return nonblockingReadBB(incoming[pid], (void **)&pd);
}


/* Called before any other methods, to allow you to initialise */
/* data structures and start any internal threads.             */
/* Arguments:                                                  */
/*   nd: the NetworkDevice that you must drive,                */
/*   mem_start, mem_length: some memory for PacketDescriptors  */
/*   fpds_ptr: You hand back a FreePacketDescriptorStore into  */
/*             which you have put the divided up memory        */
/* Hint: just divide the memory up into pieces of the right size */
/*       passing in pointers to each of them                     */
void init_network_driver(NetworkDevice *nd,
                         void *mem_start,
                         unsigned long mem_length,
                         FreePacketDescriptorStore **fpds_ptr)
{

    pthread_t sender, receiver;
    int i;

    /* Create Free Packet Descriptor Store */
    *fpds_ptr = create_fpds();
    fpds_t = *fpds_ptr;
    nd_t = nd;

    /* Load FPDS with packet descriptors constructed from mem_start/mem_length */
    (void)create_free_packet_descriptors(fpds_t, mem_start, mem_length);

    /* Create any buffers required by your thread[s] */
    for (i = 0; i < (MAX_PID + 1); i++)
        incoming[i] = createBB(IN_BUFFER);
    outgoing = createBB(MAX_PID);

    /* Create any threads you require for your implementation */
    pthread_create(&sender, NULL, &sendFxn, NULL);
    pthread_create(&receiver, NULL, &recvFxn, NULL);
}

/*
 * Function for sending packets to the network device.
 *
 * Simply obtains a packet descriptor from the outgoing buffer, then
 * attempts to send it N number of times, reporting the success/failure
 * of the sent packet. Then returns the packet descriptor back to the
 * FPDS, as it always should, regardless of a successful or failed send.
 */
#define MAX_TRIES 3     /* Times to attempt to send packet before giving up */
static void *sendFxn(UNUSED void *arg) {

    PacketDescriptor *pd;
    int i, sent;

    while (1) {

        /* Obtain packet descriptor from outgoing BB */
        if (!nonblockingReadBB(outgoing, (void **)&pd))
            pd = blockingReadBB(outgoing);

        /* Attempt to send the packet up to the specified maximum times */
        for (i = 1; i <= MAX_TRIES; i++) {
            if ((sent = send_packet(nd_t, pd)))
                break;
        }

        /* Report the packet sending success/failure */
        if (sent)
            DIAGNOSTICS("[DRIVER> Info: Sent a packet after %d tries\n", i);
        else
            DIAGNOSTICS("[DRIVER> Info: Failed to send a packet after %d tries\n", MAX_TRIES);

        /* Always returns the packet descriptor back to the store, regardless of success or failure */
        if (!nonblocking_put_pd(fpds_t, pd))
            blocking_put_pd(fpds_t, pd);
    }

    return NULL;
}

/*
 * Function for receiving packets from the network device.
 *
 * Retrieves the packet descriptor from the buffer corresponding to
 * the application. The packet is then registered with the device, waits
 * for the packet from the device, then puts into the appropriate outgoing
 * buffer to be sent. Also must always return packet descriptor back to FPDS.
 */
static void *recvFxn(UNUSED void *args) {

    PacketDescriptor *pd;
    PID pid;

    while (1) {
        
        /* Get the next packet descriptor from the store */
        if (!nonblocking_get_pd(fpds_t, &pd))
            blocking_get_pd(fpds_t, &pd);

        /* First, reset the packet to empty, since it says this should be done before registration */
        /* Next, register the packet with network device; it will use it for next incoming data packet */
        /* Then wait (block) until the packet descriptor is filled with the data packet */
        /* Finally, get its PID for writing it to the outgoing buffer */
        init_packet_descriptor(pd);
        register_receiving_packetdescriptor(nd_t, pd);
        await_incoming_packet(nd_t);
        pid = packet_descriptor_get_pid(pd);
        DIAGNOSTICS("[DRIVER> Info: Packet received for application %u\n", pid);

        /* Write the packet descriptor to the outgoing buffer */
        if (!nonblockingWriteBB(incoming[pid], pd))
            blockingWriteBB(incoming[pid], pd);

        /* Returns the packet descriptor back to the store */
        if (!nonblocking_put_pd(fpds_t, pd))
            blocking_put_pd(fpds_t, pd);
        
    }

    return NULL;
}

