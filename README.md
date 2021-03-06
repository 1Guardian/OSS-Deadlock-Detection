# IMPORTANT NOTES

This project will not work under g++ 4.8.5 (regex issues with stringstream)

structures.h contains all data-structures and contains the #define 
variables. There are many shared memory segments and the program relies on 
all of them to run.

# Notes

Inside of the log file, there are a few different messages that can appear.
The first of them is a resource request message, which looks like this:

    ResourceManager has detected Process 0 requesting R0 at time 48:2652290 
    ResourceManager running deadlock avoidance at this time

From here, there are two messages that can appear, either a granted or a denied,
which looks like this:

    Safe state after running Safety Detection algorithm
	ResourceManager granting Process 0 request R0 at time 48:2652390

or

    Unsafe state after running Safety Detection algorithm
	ResourceManager not granting Process 0 request R0 at time 48:2652490

If a process releases a resource, then OSS will log that release, which looks 
like this:

    ResourceManger has acknowledged Process P0 releasing an instance of Resource R0 at time 48:2668975

If a process gets unblocked, the OSS logs a line that looks like this:

    ResourceManager has unblocked Process 0 and granted request R0 at time 49:332804313 

When a process terminates, OSS logs it's termination which looks like this:

    Process P0 terminated

When deadlock recovery is called, OSS will log that it called it, and if any
processes can be freed by killing another process, then it will log which process
is singaled to exit to recover the system, which looks like this:

    ResourceManager calling Deadlock Recovery Process
    ResourceManager signaling Process P0 for termination

    ResourceManager has Terminated Process 0
        Resources released: 


# OSS and user_proc (Test Process): Overview

OSS is the Operating System Simulator. It acts as a simulated OS and
resource manager implemented via a preemptive checking/running of a safety
algorithm. It takes a single parameter to control whether or not it prints
the entire resource table in the output log, or just the requests and responses.
It has many message queues and shared objects that control the communication between 
this program and the child processes. It accesses a shared memory object called the
Resource Manager to decide on which requests to grant or deny. It will continue
to grant or deny requests, collecting returned resources and checking the blocked
queue periodically. Once all children are done, OSS cleans up and prints the final 
running statistics.

user_proc is the binary that actually runs as a service that our OSS
manages. It pretends to do work and then immediately exit. It periodically 
sends a message back to OSS containing if it wants to acquire a resource or 
wants to return a resource. It continues until it reaches a termination, 
decided by a random number between 0-1.0 with preference held from .33.  


# Getting Started (Compiling)

To compile both the OSS and user_proc programs, simple use of 'make'
will compile both for your use. 'make clean' will remove the created object files and the
executable binary that was compiled using the object files
