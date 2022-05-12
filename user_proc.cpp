/***********************************************************************
user_proc code for project 5:

Author: Tyler Martin
Date: 12/1/2021

-user_proc is the binary that actually runs as a service that our OSS
 manages. It pretends to do work and then immediately exit. It periodically 
 sends a message back to OSS containing if it wants a resource or wants to,
 return a resource. It continues until it reaches a termination, decided by 
 a random number between 0-1.0 with preference held from .33.  

Testing steps:
Program accesses the shared memory structs and message queues and receives
its PID. It then requests resources at random times or returns then, and waits
until it gets a message back and repeats until exit. It cleans up on termination.
***********************************************************************/

#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <random>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <time.h>
#include <ios>
#include <string>
#include <vector>
#include <tuple>
#include "string.h"
#include "sys/msg.h"
#include "structures.h"

//key for address location
#define SHM_KEY 0x98273
#define PARENT_KEY 0x94512
#define TURN_KEY 0x95712
#define KEY 0x00043 //child-to-parent key
#define ResourceMatrices_KEY 0x46583
#define SCHEDULER_KEY 0x46513
#define ResourceQueue_KEY 0x71623

//perror header
std::string exe;

//message object
struct message msg;

//int to hold which processID we take up
int ProcessID = 0;

/***********************************************************************
 create shared message queues 

 Queues: 
        Queue for child-to-parent communication
        Queue for parent check-up
        Queue for all-to-resource matrices communication
        Queue for talking to scheduler
***********************************************************************/
int msgid = msgget(KEY, 0666 | IPC_CREAT); 

int parent_msgid = msgget(PARENT_KEY, 0666 | IPC_CREAT);

int resource_msgid = msgget(ResourceQueue_KEY, 0666 | IPC_CREAT); 

int returning_msgid = msgget(SCHEDULER_KEY, 0666 | IPC_CREAT); 

/***********************************************************************
 create shared message memory segments 

 Objects:
        Clock Object for time management
        Turn Management Semaphore for starting children
        The resource manager
***********************************************************************/
int shmid = shmget(SHM_KEY, sizeof(struct Clock), 0600|IPC_CREAT);
struct Clock *currentClockObject = (static_cast<Clock *>(shmat(shmid, NULL, 0)));

int Turn_shmid = shmget(TURN_KEY, sizeof(struct TurnSemaphore), 0600|IPC_CREAT);
struct TurnSemaphore *currentTurnSemaphore = (static_cast<TurnSemaphore *>(shmat(Turn_shmid, NULL, 0)));

int Allocated_shmid = shmget(ResourceMatrices_KEY, sizeof(struct ResourceMatrices), 0600|IPC_CREAT);
struct ResourceMatrices *Resources = (static_cast<ResourceMatrices *>(shmat(Allocated_shmid, NULL, 0)));

/***********************************************************************
Function: SafelyFormulateRequest()

Description: Function that employs message queues to ensure no two 
             processes end up in the resource matrices at the same
             time. Requests a resource from the manager
***********************************************************************/
int SafelyFormulateRequest(int ProcessID){
    //safely access queue
    if (msgrcv(resource_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //formulate our request
    int request = Resources->FormulateRequest(ProcessID);

    //Cede potential control back
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

    return request;
}

/***********************************************************************
Function: SafelyFormulateReturn()

Description: Function that employs message queues to ensure no two 
             processes end up in the resource matrices at the same
             time. requests a resource return to the manager
***********************************************************************/
int SafelyFormulateReturn(int ProcessID){
    //safely access queue
    if (msgrcv(resource_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //formulate our request
    int request = Resources->FormulateReturn(ProcessID);

    //Cede potential control back
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

    return request;
}

/***********************************************************************
Function: safelyCheckResources()

Description: Function that employs message queues to ensure no two 
             processes end up in the resource matrices at the same
             time. Checks the resource that this process has allocated
***********************************************************************/
bool safelyCheckResources(int ProcessID){
    //safely access queue
    if (msgrcv(resource_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //formulate our request
    bool retval = Resources->hasResources(ProcessID);

    //Cede potential control back
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

    return retval;
}

/***********************************************************************
Function: sendCleanupMessage()

Description: Function that signals OSS through a message queue to let it
             know that this process is cleaning up and exiting. It has all
             of it's resources reaped when OSS acknowledges the exit.
***********************************************************************/
void sendCleanupMessage(){

    //let OSS know which process is cleaning up and exiting
    std::string blocked = std::to_string(ProcessID);
    std::string clean = "                          "; 
    clean.copy(msg.text, 200);
    blocked.copy(msg.text, 200);
    
    //Send a request for a resource to the OSS
    if(msgsnd(parent_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        std::cout << "Can't send request!\n";
        perror(&exe[0]);
    }
}

/***********************************************************************
Function: determineProcessIDandInstantiate()

Description: Function that employs message queues to ensure no two 
             processes end up in the maximum resource matrices at the same
             time. Acts as the first setup of the process in the max resources
             table as well as the allocated table
***********************************************************************/
void determineProcessIDandInstantiate(){
    //safely access queue
    if (msgrcv(resource_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //log processID
    ProcessID = Resources->getProcessIDandSetArrays();

    //Cede potential control back
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

}

/***********************************************************************
Function: int doWork(std::vector<int>)

Description: Function that executes when child is put into the running
             state as indicated by the shared semaphore that controls
             the running PID. it requests random resources or returns 
             random resources, and waits for confirmation. If it does not 
             terminate, then it needs to decide whether or not it will request
             again or exit on next cycle.
***********************************************************************/
void doWork(std::vector<int> maxClaimsVector){
    
    //keep track of whether or not we terminated
    bool terminated = false;
    int i = 0;
    int fulfilled = 0;

    //random seeds and generators
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<double> requestTimeRange(0.0, 250000000.0);
    std::uniform_real_distribution<double> exit(0.0, 10.0);

    //get time to new request
    double requestTime = requestTimeRange(mt);
    requestTime /= 1000000000;

    //Get process Start Time
    double startingStamp = currentClockObject->getFractionalStamp();
    double previousStamp = currentClockObject->getFractionalStamp();

    //set variable for blocked time logging
    std::string blockedStart="0";

    while(!terminated){

        //get current time
        double currentStamp = currentClockObject->getFractionalStamp();

        //if time has elapsed enough, make request
        if(currentStamp - previousStamp >= requestTime){
            
            //update pervious stamp
            previousStamp = currentClockObject->getFractionalStamp();

            //check if this process should end
            if(fulfilled > 2 && safelyCheckResources(ProcessID) && currentStamp - startingStamp > 2 && exit(mt) < 3){
                return;
            }

            //check if we should release a resource or request one
            
            //RETURN RESOURCE
            if(exit(mt) < 5){
                //get our resource request
                int resource = SafelyFormulateReturn(ProcessID);

                //if we got a -1 as return, we had no possible resources to give back, so ignore
                if(resource != -1){

                    //build our return notification
                    //use stringstream for leading zeros in PID
                    std::ostringstream ss;
                    ss << std::setw(2) << std::setfill('0') << ProcessID;
                    std::string PID(ss.str());
                    ss.str("");

                    //use stringstream for leading zeros in resource
                    ss << std::setw(2) << std::setfill('0') << resource;
                    std::string RESOURCE(ss.str());
                    ss.str("");

                    //use stringstream for leading zeros in amount
                    ss << std::setw(2) << std::setfill('0') << 1;
                    std::string AMOUNT(ss.str());
                    ss.str("");

                    //build request into string and copy to msg
                    std::string blocked = PID + std::string(":") + RESOURCE + std::string(":") + AMOUNT;
                    blocked.copy(msg.text, 200);
                    
                    //Send a request for a resource to the OSS
                    if(msgsnd(returning_msgid, &msg, strlen(msg.text)+1, 0) == -1){
                        std::cout << "Can't send request!\n";
                        perror(&exe[0]);
                    }
                }
            }
            //REQUEST RESOURCE
            else{
                //get our resource request
                int resource = SafelyFormulateRequest(ProcessID);
                                
                //build our request
                //use stringstream for leading zeros in PID
                std::ostringstream ss;
                ss << std::setw(2) << std::setfill('0') << ProcessID;
                std::string PID(ss.str());
                ss.str("");

                //use stringstream for leading zeros in resource
                ss << std::setw(2) << std::setfill('0') << resource;
                std::string RESOURCE(ss.str());
                ss.str("");

                //use stringstream for leading zeros in amount
                ss << std::setw(2) << std::setfill('0') << 1;
                std::string AMOUNT(ss.str());
                ss.str("");

                //build request into string and copy to msg
                std::string blocked = PID + std::string(":") + RESOURCE + std::string(":") + AMOUNT;
                blocked.copy(msg.text, 200);
                
                //Send a request for a resource to the OSS
                if(msgsnd(msgid, &msg, strlen(msg.text)+1, 0) == -1){
                    std::cout << "Can't send request!\n";
                    perror(&exe[0]);
                }  

                fulfilled+=1;
                //wait for response
                while(!currentTurnSemaphore->getTurn(ProcessID)){
                    if(currentTurnSemaphore->getTurn(-2)) //worst case, something went wrong, program can be told to terminate
                        return;
                    if(currentTurnSemaphore->getKill(ProcessID)){ //if a blocked process is deemed necessary to kill to resolve the deadlock, make it die
                        currentTurnSemaphore->setKill(-1); //let parent take control back
                        return;
                        }
                };

                //return control to oss
                currentTurnSemaphore->setTurn(-1);
            }
        } 

        //save a bit of time and return if time
        if(terminated){
            return;
        }
    }
}

int main(int argc, char *argv[]){

    //Get the perror header
    std::ifstream("/proc/self/comm") >> exe;
    exe.append(": Error");

    //Central maximum claims table we can reference
    std::vector<std::vector<int>> maximumptr;
    std::vector<int> maxClaimsVector;

    //check all memory allocations
    if(shmid == -1) {
        std::cout <<"Pre-Program Memory allocation error. Program must exit.\n";
        errno = 11;
        perror(&exe[0]);
    }

    //make our max claims vector and share it asap
    determineProcessIDandInstantiate();

    //actually do work when allowed
    doWork(maxClaimsVector);

    //release all resources that may still be held
    sendCleanupMessage();

    //spinlock to make sure OSS understands this process is leaving
    int tracker = 0;
    while(tracker < 300000){
        tracker++;
        if(currentTurnSemaphore->getTurn(ProcessID)){
            currentTurnSemaphore->setTurn(-1);
        }
        if(currentTurnSemaphore->getKill(ProcessID)){
            currentTurnSemaphore->setKill(-1);
        }
    }

    //close memory
    shmdt((void *) currentTurnSemaphore);
    shmdt((void *) currentClockObject);
    shmdt((void *) Resources);

    exit(EXIT_SUCCESS);
}