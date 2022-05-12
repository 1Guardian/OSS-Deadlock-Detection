/***********************************************************************
OSS code for project 5:

Author: Tyler Martin
Date: 12/1/2021

- OSS is the Operating System Simulator. It acts as a simulated OS and
  resource manager implemented via a preemptive checking/running of a safety
  algorithm. It takes a single parameter to control whether or not it prints
  the entire resource table in the output log, or just the requests and responses.
  It has many message queues and shared objects that control 
  the communication between this program and the child processes. It starts 
  by creating the queues and spawning the first child. That child will note
  it's PID similar to my last project, but this time the PIDS only range from
  0-17 since the children only fill up the slots in the process table (18 slots).
  From here, the child will begin requesting resources randomly via a message queue
  and the parent (OSS) will run the safety algorithm on it to determine if it is safe
  to grant the request. If the request is safe, then it is granted. If the request returns
  an unsafe state, then the request is denied. The parent will continue to spawn new processes
  until there are 18 spawned children at the same time, or until 5 seconds elapses. When all
  children are done, they clean up and return the resources. If a child gets rejected 
  with respect to it's request, it waits in a blocked queue until it can be granted. If the
  blocked queue becomes deadlocked with the children all requesting things that can't be granted, 
  then OSS will kill children for deadlock recovery. The method used to determine which children
  to reap is simply FIFO. One all children are done, OSS cleans up and prints the final
  running statistics.

  First time I have ever programmed from inside of vim instead of any visual studio codium 
  editor. 

  This is a bit weird, but I see the alure of using it. 


Testing steps:
Program allocates shared memory and message queues. It then creates two 
additional threads and begins forking children while running a safety
algorithm on each request until it runs out of processes
or time elapsing. It logs all events while it executes and prints general
statistics at the end of the program's execution.
***********************************************************************/
#include <stdio.h>
#include <sys/ipc.h>
#include <signal.h>
#include <sys/shm.h>
#include <bits/stdc++.h>
#include <sys/types.h>
#include <algorithm>
#include <regex>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>
#include <chrono>
#include <random>
#include <vector>
#include <thread>
#include <tuple>
#include "sys/msg.h"
#include "string.h"
#include "structures.h"

//key for address location
#define SHM_KEY 0x98273
#define PARENT_KEY 0x94512
#define PCT_KEY 0x92612
#define TURN_KEY 0x95712
#define KEY 0x00043 //child-to-parent key
#define CLOCK_KEY 0x05912
#define PCB_KEY 0x46781
#define ResourceMatrices_KEY 0x46583
#define SCHEDULER_KEY 0x46513
#define FINAL_STATS_KEY 0x396561
#define ResourceQueue_KEY 0x71623
#define MaxClaimsQueue_KEY 0x19238

bool amChild = false;

//sentinel to control whether or not to 
//print out the resource allocation matrix
bool verbose = false;

//number of running children
int runningChildren=0;

//argv[0] perror header
std::string exe;

//message object
struct message msg;

//logfile name and max exec time
std::string logfile = "logfile";
int maxExecutionTime = 120;

/***********************************************************************
 create shared message queues 

 Queues: 
        Queue for child-to-parent communication
        Queue for parent check-up
        Queue for all-to-resource matrices communication
        Queue for all-to-maximum resource communication
        Queue for PCB access
        Queue for talking to scheduler
***********************************************************************/
int msgid = msgget(KEY, 0666 | IPC_CREAT); 

int parent_msgid = msgget(PARENT_KEY, 0666 | IPC_CREAT);

int resource_msgid = msgget(ResourceQueue_KEY, 0666 | IPC_CREAT); 

int maximum_msgid = msgget(MaxClaimsQueue_KEY, 0666 | IPC_CREAT); 

int PCB_msgid = msgget(PCB_KEY, 0666 | IPC_CREAT); 

int returning_msgid = msgget(SCHEDULER_KEY, 0666 | IPC_CREAT); 

/***********************************************************************
 create shared message memory segments 

 Objects:
        Clock Object for time management
        Turn Management Semaphore for starting children
        The Resource Manager Object
        The PIDS vector (made shared even though only runsim wil ever use it
                         so that all threads see it)
***********************************************************************/
int shmid = shmget(SHM_KEY, sizeof(struct Clock), 0600|IPC_CREAT);
struct Clock *currentClockObject = (static_cast<Clock *>(shmat(shmid, NULL, 0)));

int Turn_shmid = shmget(TURN_KEY, sizeof(struct TurnSemaphore), 0600|IPC_CREAT);
struct TurnSemaphore *currentTurnSemaphore = (static_cast<TurnSemaphore *>(shmat(Turn_shmid, NULL, 0)));

int Allocated_shmid = shmget(ResourceMatrices_KEY, sizeof(struct ResourceMatrices), 0600|IPC_CREAT);
struct ResourceMatrices *Resources = (static_cast<ResourceMatrices *>(shmat(Allocated_shmid, NULL, 0)));

int PIDSVec_shmid = shmget(/*key defined here cause it isn't needed by any other process*/0x571254, sizeof(struct PIDSVector), 0600|IPC_CREAT);
struct PIDSVector *PIDSVec = (static_cast<PIDSVector *>(shmat(PIDSVec_shmid, NULL, 0)));

/***********************************************************************
Function: SafelyStartSafetyAlgorithm()

Description: Function that employs message queues to ensure no two 
             processes end up in the resource matrices at the same
             time, then it requests that the safety algorithm be run
***********************************************************************/
std::string SafelyStartSafetyAlgorithm(int resource, int resourceAmountRequested, int processID){
    //safely access queue
    if (msgrcv(resource_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //do the safety and allocation
    std::string safe = Resources->StartSafetyAlgorithm(resource, resourceAmountRequested, processID);

    //Cede potential control back
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

    return safe;
}

/***********************************************************************
Function: SafelyGetAllocationMatrix()

Description: Function that employs message queues to ensure no two 
             processes end up in the resource matrices at the same
             time, then it requests the allocation matrix for review
***********************************************************************/
std::string SafelyGetAllocationMatrix(){
    //safely access queue
    if (msgrcv(resource_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //do the safety and allocation
    std::string safe = Resources->show();

    //Cede potential control back
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

    return safe;
}

/***********************************************************************
Function: SafelystartBlockedCheck()

Description: Function that employs message queues to ensure no two 
             processes end up in the resource matrices at the same
             time, then it starts checking the blocked queue for possible
             unblocks.
***********************************************************************/
std::string SafelystartBlockedCheck(){
    //safely access queue
    if (msgrcv(resource_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //do the safety and allocation
    std::string results = Resources->startBlockedCheck();

    //Cede potential control back
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

    return results;
}

/***********************************************************************
Function: SafelyStartDeadlockRecovery()

Description: Function that employs message queues to ensure no two 
             processes end up in the resource matrices at the same
             time, then it begins evaluating which processes can be
             terminated to free another process
***********************************************************************/
std::string SafelyStartDeadlockRecovery(){
    //safely access queue
    if (msgrcv(resource_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //do the safety and allocation
    std::string results = Resources->StartDeadlockRecovery();

    //Cede potential control back
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

    return results;
}

/***********************************************************************
Function: void safelyReturnResources(int)

Description: Function that executes when children return resources. 
             The child sends a message to OSS to return a resource, 
             which makes OSS contact the resource manager and tells 
             it to return the resource taken by the child.
***********************************************************************/
std::string safelyReturnResources(int ProcessID){

    //safely access queue
    if (msgrcv(resource_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //formulate our request
    std::string returned = Resources->ReturnResources(ProcessID);

    //Cede potential control back
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

    return returned;
    
}

/***********************************************************************
Function: void safelyCallUnblock(int)

Description: Function that OSS executes when a child can be unblocked.
             this will grant the child the request it was waiting on and
             unblocks it.
***********************************************************************/
std::string safelyCallUnblock(int ProcessID){

    //signal the process
    currentTurnSemaphore->setTurn(ProcessID);

    while(!currentTurnSemaphore->getTurn(-1));

    //safely access queue
    if (msgrcv(resource_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //formulate our request
    std::string returned = Resources->UnblockProcess(ProcessID);

    //Cede potential control back
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

    return returned;
    
}

/***********************************************************************
Function: void safelyCallTermination()

Description: Function that executes when child needs to be terminated   
             in deadlock recovery. It calls resource manager to determine
             which child should be killed. Using message queues for safe 
             access.
***********************************************************************/
std::string safelyCallTermination(int ProcessID){

    //signal the process
    currentTurnSemaphore->setKill(ProcessID);

    while(!currentTurnSemaphore->getKill(-1));

    //safely access queue
    if (msgrcv(resource_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        perror(&exe[0]);
    }

    //formulate our request
    std::string returned = Resources->TerminateProcess(ProcessID);

    //Cede potential control back
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

    return returned;
    
}

/***********************************************************************
Function: docommand(char*)

Description: Function to call the grandchildren 
             (user_proc). Uses execl to open a bash
             instance and then simply run the 
             program by providing the name of the bin
             and the relative path appended to the 
             beginning of the command.
***********************************************************************/
void docommand(char* command){

    //add relative path to bin
    std::string relativeCommand = "./";
    relativeCommand.append(command);

    //execute bin
    execl("/bin/bash", "bash", "-c", &relativeCommand[0], NULL);
}

/***********************************************************************
Function: deleteMemory(char*)

Description: Function to delete all shared memory
***********************************************************************/
void deleteMemory(){

    //delete each memory/queue share
    shmctl(shmid, IPC_RMID, NULL);
    msgctl(msgid, IPC_RMID, NULL);
    shmctl(PIDSVec_shmid, IPC_RMID, NULL);
    msgctl(parent_msgid, IPC_RMID, NULL);
    msgctl(resource_msgid, IPC_RMID, NULL);
    msgctl(PCB_msgid, IPC_RMID, NULL);
    msgctl(maximum_msgid, IPC_RMID, NULL);
    shmctl(Turn_shmid, IPC_RMID, NULL);
    msgctl(returning_msgid, IPC_RMID, NULL);
    shmctl(Allocated_shmid, IPC_RMID, NULL);
}

/***********************************************************************
Function: threadReturn()

Description: Function that a thread will inhabit with
             the sole purpose and intent of keeping 
             track of which child processes are still
             running
***********************************************************************/
void threadReturn()
{
    while(1){
        if(waitpid(-1, NULL, WNOHANG) > 0){
            PIDSVec->PIDS.pop_back();
        }
    }
}

/***********************************************************************
Function: cleanUp(bool)

Description: Function that receives control of the OSS program
             when the main scheduler is finished. It's job is to
             cleanup and delegate the deletion of shared memory, and make
             sure that no zombie processes are found.
             (Functionally a copy of threadkill)
***********************************************************************/
void cleanUp(bool show = true)
{
    //wait for children that are still de-coupling
    while(wait(NULL) > 0);

    //kill all children
    for(int i=0; i < PIDSVec->PIDS.size(); i++)
        kill(std::get<0>(PIDSVec->PIDS[i]), SIGTERM);

    //clear memory 
    deleteMemory();
    exit(1);
}

/***********************************************************************
Function: siginthandler(int)

Description: Function that receives control of
             termination when program has received 
             a termination signal
***********************************************************************/
void siginthandler(int param)
{
    //kill all children
    for(int i=0; i < PIDSVec->PIDS.size(); i++)
        kill(std::get<0>(PIDSVec->PIDS[i]), SIGTERM);

    //clear memory
    deleteMemory();
    exit(1);
}

/***********************************************************************
Function: threadKill(std::chrono::steady_clock::time_point)

Description: Function that receives control of
             termination when program has received 
             a termination signal from the time 
             contained in config.h being ellapsed.
***********************************************************************/
void threadKill(std::chrono::steady_clock::time_point start, int max_run_time)
{
    //don't execute until time has ellapsed.
    while((std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < maxExecutionTime) && amChild != true);

    //signal both scheduler and all children that exit time has arrived
    currentTurnSemaphore->setTurn(-2);

    //finish cleaning up and exit
    cleanUp();
}

/***********************************************************************
Function: printStats()

Description: Prints the final running statistics of the Resource Manager

***********************************************************************/
void printStats(){

    //stats vars 
    int grantedRequestsImmediately =0;
    int grantedRequestsWaiting =0;
    int finishedProcesses =0;
    int terminatedProcesses =0;
    int timesDeadlockDetectionRan =0;
    int processesInBlock =0;
    int recovery =0;

    std::cout << std::endl << "Operating System Simulator (Resource Manager) End of Run Statistics: " << std::endl;
    std::cout << "==========================================================================" << std::endl;

    // filestream variable file
    std::fstream file;
    std::string word, t, q;
  
    // opening file
    file.open(logfile.c_str());
  
    // extracting words from the file
    while (file >> word)
    {
        if(word == "terminated") //process finished
            finishedProcesses++;

        if(word == "termination") //process terminated
            terminatedProcesses++;

        if(word == "unblocked") //processes granted request after waiting
            grantedRequestsWaiting++;

        if(word == "Safe") //processes granted request immediately
            grantedRequestsImmediately++;

        if(word == "Detection") //times deadlock detection ran
            timesDeadlockDetectionRan++;
        
        if(word == "Unsafe") //processes blocked
            processesInBlock++;

        if(word == "Recovery")
            recovery++;
    }

    std::cout << "Processes That Finished Without Termination: " << finishedProcesses - terminatedProcesses << std::endl;
    std::cout << "Processes That Were Terminated: " << terminatedProcesses << std::endl;
    std::cout << "Processes That Were Granted Resources Immediately: " << grantedRequestsImmediately << std::endl;
    std::cout << "Processes That Were Granted Resources After Waiting: " << processesInBlock - terminatedProcesses << std::endl;
    std::cout << "Times Deadlock Detection Ran: " << timesDeadlockDetectionRan << std::endl;
    if(processesInBlock > 0){
        int finalaveragestat = (static_cast<float>(terminatedProcesses) / static_cast<float>(processesInBlock)) * 100.0;
        std::cout << "Average Percent of Processes Terminated From Deadlock: " << finalaveragestat << "%" << std::endl;
    }
    else 
        std::cout << "Average Percent of Processes Terminated From Deadlock: 0%" << std::endl;
    cleanUp(false);
}

/***********************************************************************
Function: convertResourceRequest(std::string)

Description: Function that splits the resource request message
***********************************************************************/
void convertResourceRequest(std::string stamp, int* processID, int* resource, int* resourceAmountRequested){

    //convert the colons to spaces for ss
    std::replace(stamp.begin(), stamp.end(), ':', ' ');

    //ss operators and holding variables
    std::vector<unsigned int> array;
    std::stringstream ss(stamp);
    unsigned int temp;

    //read string into vector
    while (ss >> temp)
        array.push_back(temp);

    //return values via pointer
    *processID = array[0];
    *resource = array[1];
    *resourceAmountRequested = array[2];

}

/***********************************************************************
Function: childProcess(std::string, pid_t, int)

Description: Function that executes docommand and 
             waits for child process to finish.
***********************************************************************/
void childProcess(std::string currentCommand, pid_t wpid, int status){
    //child code
    amChild = true;

    //run docommand
    docommand(&currentCommand[0]);
}

/***********************************************************************
Function: checkGranted(std::string)

Description: Function that checks if a child was granted a resource for
             logging purposes.
***********************************************************************/
bool checkGranted(std::string retval){

    //ss operator for loop
    std::stringstream ss(retval);
    std::string tmp;

    //check for access granted
    while(ss >> tmp){
        if(tmp == "Safe"){
            return true;
        }
    }

    return false;

}

/***********************************************************************
Function: initTable(int, int)

Description: Function that initializes the process control table
             for the new process which has just confirmed it's creation
***********************************************************************/
void initTable(int child_pid, int count){

    //since initialization passed, setup a PCT PCB for the new child
    //ask for permission
    if (msgrcv(PCB_msgid, &msg, sizeof(msg), 0 ,0) == -1){
        std::cout << "stuck waiting for permission to look at PCB" << std::endl;
        perror(&exe[0]);
    }

    //Since initialization passed, push pid to stack
    PIDSVec->PIDS.push_back(std::make_tuple (child_pid, count));

    //cede access to the PCB back
    if(msgsnd(PCB_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        std::cout << "Can't cede control back to worker from PCB thread!\n";
        perror(&exe[0]);
    }
}

/***********************************************************************
Function: scheduleNewProcess(pid_t*, pid_t*, int*, int*)

Description: Function that executes the scheduling initilization duties
             creates children as well as assigns pids and
             waits for confimation from children
***********************************************************************/
void scheduleNewProcess(pid_t* child_pid, pid_t* wpid, int* status, int* count){

    //keep going until we hit max allowed processes (40 default)
    if(*count < MAX_PROCESSES){ 

        //don't let more than 20 processes run at any time
        if(PIDSVec->PIDS.size() < 18){

            //check to see if process is child or parent
            if((*child_pid = fork()) < 0){
                perror(&exe[0]);
            }
            if (*child_pid  == 0) {
                //start child process
                childProcess("user_proc", *wpid, *status);
            }

            //init the table entry
            initTable(*child_pid, *count);

            //update count
            *count = *count + 1;
            
        }
    }
}

/***********************************************************************
Function: instantiateMatrices()

Description: Function that fills the matrices with initial resource values
             and ensures that the allocated matrix is initialized with 
             null values so that the child processes know which matrix
             to allocate from 
***********************************************************************/
void instantiateMatrices(){

    //pass the resource and process amount values to the function
    Resources->instantiateMatrix(R, P);
}

/***********************************************************************
Function: handleRequests(pid_t*, pid_t*, int*)

Description: Function that actually handles all resource allocation or
             denials of requests. The request comes in via a message
             detailing what the process needs, and then bankers' 
             alorithm is run to determine whether or not the process
             will get it's request granted.
***********************************************************************/
void handleRequests(pid_t* child_pid, pid_t* wpid, int* instatus){
    //hold status && PID
    int tmppid, maskedPID;
    int flipbit = 0;
    bool hasDoneWork = false;
    int count = 0, verbosecount=0;

    //more modern use of C++11
    //random seeds and generators
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<double> blockedRand(1.0, 100000.0);
    std::uniform_real_distribution<double> timeToNewProcess(0.0, 500000.0);

    //variables to write to output file
    std::ofstream outdata;
    outdata.open(logfile);

    //variable to hold the timer before a new process is allowed to spawn
    double spawnTime = timeToNewProcess(mt);
    double workedStamp = 0;

    //initial starting time for use during averages and timing the spawning of children
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    //vectors that hold the values of resources available, process ids, and allocated resources
    std::vector<int> available;
    std::vector<std::vector<int>> maximum;
    std::vector<std::vector<int>> allocated;

    //struct to hold all of our ctl params for our queues
    struct msqid_ds msgid_stats;
    struct msqid_ds returning_msgid_stats;
    struct msqid_ds Parent_msgid_stats;

    //sentinel to save on execution time
    bool timerElapsed = false;
    bool deadlocked = false;

    //line tracker
    int lineCheck =0;

    //fill our matrices
    instantiateMatrices();

    //string to handle a bug that happens at the end of logging
    //(Read README to see full explanation)
    std::string lastLine;

    //Print out Program Header
    std::cout << "CS 4760 Project 5 (Resource Manager)" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "File Being Logged To: " << logfile << std::endl;
    std::cout << "Verbose Mode Enabled: " << verbose << std::endl;
    std::cout << "Resource Count: " << 20 << std::endl;
    std::cout << "Process Count: " << 18 << std::endl;
    fputs("====================================\n\nBeginning Run: .", stdout);
    fflush(stdout);


    //simple constant loop to keep our process in the context of this loop
    while(1){  
        
        //if it's time to end, return
        if (PIDSVec->PIDS.empty() && timerElapsed || PIDSVec->PIDS.size() == 1 && timerElapsed){
            outdata << "ResourceManager: All Requests Handled. Exiting." << std::endl;

            //signal any zombies that it's time to exit
            currentTurnSemaphore->setTurn(-2);
            return;
        }

        //schedule new process if possible 
        //condition that prevents even entering is if time has elapsed
        if((std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() <= MAX_INITIALIZATION_TIME)){
            scheduleNewProcess(child_pid, wpid, instatus, &count);
        }
        else{
            //if timer is in excess of 5 seconds, stop spawning processes
            timerElapsed = true;
        }   

        //======================================================
        //
        // NEW INCOMING PROCESS TERMINATIONS
        //
        //======================================================
        //check the queue to see if we have a request waiting or if we can tend to something else
        if( msgctl(parent_msgid, IPC_STAT, &Parent_msgid_stats) == -1)
            perror(&exe[0]);

        if(Parent_msgid_stats.msg_qnum > 0){

            //print activity notification
            fputs(".", stdout);
            fflush(stdout);

            //Immediately take all resources that are being returned
            int msgNum = Parent_msgid_stats.msg_qnum;
            for(int i =0; i < msgNum; i++){
                //Wait for a request to come through msg queue
                //FIXME: might want to change the message channel the 
                //request comes through later (for tidyness)
                if (msgrcv(parent_msgid, &msg, sizeof(msg), 0 ,0) == -1){
                    perror(&exe[0]);
                }

                //get request
                std::string currentRequest(msg.text);

                //call the cleanup via the safe accessor
                std::string retVals = safelyReturnResources(stoi(currentRequest));

                //log the resources that were returned
                if(lineCheck < 100000){
                    lineCheck++;
                    outdata << "Process P" << stoi(currentRequest) << " terminated" << std::endl << std::endl;
                    lastLine = "null";
                }
            }
        }

        //======================================================
        //
        // NEW INCOMING RESOURCE RETURNS
        //
        //======================================================
        //check the queue to see if we have a request waiting or if we can tend to something else
        if( msgctl(returning_msgid, IPC_STAT, &returning_msgid_stats) == -1)
            perror(&exe[0]);

        if(returning_msgid_stats.msg_qnum > 0){

            //print activity notification
            fputs(".", stdout);
            fflush(stdout);

            //Immediately take all resources that are being returned
            int msgNum = returning_msgid_stats.msg_qnum;
            for(int i =0; i < msgNum; i++){
                //Wait for a request to come through msg queue
                //FIXME: might want to change the message channel the 
                //request comes through later (for tidyness)
                if (msgrcv(returning_msgid, &msg, sizeof(msg), 0 ,0) == -1){
                    perror(&exe[0]);
                }

                //get request
                std::string currentRequest(msg.text);

                //variables to hold the request numerics
                int processID;
                int resource;
                int resourceAmountRequested;

                //function to split the string and assign the values
                convertResourceRequest(currentRequest, &processID, &resource, &resourceAmountRequested);

                //Log the returned resource
                if(lineCheck < 100000){
                    lineCheck++;
                    outdata << "ResourceManger has acknowledged Process P" << processID << " releasing an instance of Resource R" << resource << " at time " << currentClockObject->getTimeStamp() << std::endl << std::endl;
                    lastLine = "null";
                }
                
                //increase time stamp to simulate time taken to return resource
                currentClockObject->setTimeStamp(100);

                //show that work was done 
                hasDoneWork = true;

                //set the worked stamp to higher since we could free a deadlock by gaining previously held resources
                workedStamp = currentClockObject->getFractionalStamp();
            }
        }

        //======================================================
        //
        // NEW INCOMING REQUESTS
        //
        //======================================================
        //check the queue to see if we have a request waiting or if we can tend to something else
        if( msgctl(msgid, IPC_STAT, &msgid_stats) == -1)
            perror(&exe[0]);

        if(msgid_stats.msg_qnum > 0){

            //print activity notification
            fputs(".", stdout);
            fflush(stdout);

            int msgNum = msgid_stats.msg_qnum;
            for(int i =0; i < msgNum; i++){
                //Wait for a request to come through msg queue
                //FIXME: might want to change the message channel the 
                //request comes through later (for tidyness)
                if (msgrcv(msgid, &msg, sizeof(msg), 0 ,0) == -1){
                    perror(&exe[0]);
                }

                //message will request with name PID {number x2} and then the resource requested {number x2} and then the amount requested {number x2} 
                //message format: 00:00:00
                std::string currentRequest(msg.text);

                //variables to hold the request numerics
                int processID;
                int resource;
                int resourceAmountRequested;

                //function to split the string and assign the values
                convertResourceRequest(currentRequest, &processID, &resource, &resourceAmountRequested);

                //ask the resource manager to check the safety of this system configuration
                std::string outlog = SafelyStartSafetyAlgorithm(resource, resourceAmountRequested, processID);
                outlog = std::regex_replace(outlog, std::regex("xxx"), currentClockObject->getTimeStamp()); 

                //add a few nanoseconds to account for time spent checking the safety algorithm
                currentClockObject->setTimeStamp(100);
                outlog = std::regex_replace(outlog, std::regex("yyy"), currentClockObject->getTimeStamp()); 

                //log
                if(lineCheck < 100000){
                    lineCheck++;
                    outdata << outlog << std::endl;
                    lastLine = "null";
                }

                //signal to the child that it's request was granted
                if(checkGranted(outlog)){
                    if(processID != 0){
                        currentTurnSemaphore->setTurn(processID);

                        //set the time stamp that will check for a deadlock
                        hasDoneWork = true;
                        workedStamp = currentClockObject->getFractionalStamp();
                    }
                }

                //Don't do anything if the turn has not been ceded back
                while(!currentTurnSemaphore->getTurn(-1));

                //print the resource allocation matrix every 3 logs if verbose is enabled
                if(verbose && verbosecount > 19){
                    if(lineCheck < 100000){
                        lineCheck++;
                        outdata << "Current Resource Allocation Table:" << std::endl;
                        outdata << SafelyGetAllocationMatrix() << std::endl;
                        lastLine = "null";
                        verbosecount = 0;
                    }
                }
                else
                    verbosecount++;
            }
        }

        //======================================================
        //
        // CHECKING BLOCKED QUEUE
        //
        //======================================================
        //now check the blocked queue and see if we can help any processes continue
        //blocked format: Tuple <ID:RESOURCE>
        //if process is available and we have a resource to grant, give it the resource and remove from blocked queue
        if(hasDoneWork){

            //print activity notification
            fputs(".", stdout);
            fflush(stdout);

            //flip sentinel back
            hasDoneWork = false;

            //do a blocked check
            std::string results = SafelystartBlockedCheck();

            //split this string into usable process numbers
            std::stringstream ss;
            ss << results;
            std::vector<int> array;
            int tmp;
            while(ss >> tmp){
                array.push_back(tmp);
            }
            
            //call the unblock function one at a time
            for(int i =0; i < array.size(); i++){
                std::string data = safelyCallUnblock(array[i]);

                //log the data to the file if any is returned
                data = std::regex_replace(data, std::regex("zzz"), currentClockObject->getTimeStamp()); 
                if(lineCheck < 100000){
                    lineCheck++;
                    outdata << data << std::endl;
                    lastLine = "null";
                }

                //update the stamp to reflect this time as work (since it is)
                workedStamp = currentClockObject->getFractionalStamp();
            }
        }

        //======================================================
        //
        // DEADLOCK CORRECTION
        //
        //======================================================
        //we are now at a confirmed deadlock and standstill.
        //time to fix it. Works the same as before with RM
        //proposing a set of terminations and OSS actually
        // carrying out those terminations.
        if(currentClockObject->getFractionalStamp() - workedStamp > 5) {

            //log
            if(lastLine == "null" && PIDSVec->PIDS.size() > 1){
                if(lineCheck < 100000){
                    lineCheck++;
                    outdata << "ResourceManager calling Deadlock Recovery Process" << std::endl << std::endl;
                    lastLine = "bug";
                }
            }
            //print activity notification
            fputs(".", stdout);
            fflush(stdout);

            //update the stamp to reflect this time as work (since it is)
            workedStamp = currentClockObject->getFractionalStamp();

            //same as above, but with terminations instead of freeing
            //do a blocked check
            std::string results = SafelyStartDeadlockRecovery();

            //split this string into usable process numbers
            std::stringstream ss;
            ss << results;
            std::vector<int> array;
            int tmp;
            while(ss >> tmp){
                array.push_back(tmp);
            }
            
            //call the unblock function one at a time
            for(int i =0; i < array.size(); i++){
                std::string data = safelyCallTermination(array[i]);
                
                //log that we are terminating a process
                if(1){
                    if(lineCheck < 100000){
                        lineCheck++;
                        outdata << "ResourceManager signaling Process P" << array[i] << " for termination" << std::endl << std::endl;
                        lastLine = "null";
                    }
                }

                //log the data to the file if any is returned
                data = std::regex_replace(data, std::regex("zzz"), currentClockObject->getTimeStamp());
                if(lineCheck < 100000){
                    lineCheck++; 
                    outdata << data << std::endl;
                    lastLine = "null";
                }
            }

        }

        //at the end of each pass, add some time to our clock
        unsigned int timeadded = blockedRand(mt);
        currentClockObject->setTimeStamp(timeadded);
    }
}

/***********************************************************************
Function: main(int, char*)

Description: Main function. Starts by checking parameters and
             allocated memory. It then ensures that the correct 
             pthreads are created and dispatched. It then inits 
             the queue and then starts the scheduler. 
***********************************************************************/
int main(int argc, char *argv[])
{
    //Handle Command Line Switches
    int opt;
    while ((opt = getopt(argc, argv, ":v")) != -1) {
        switch (opt) {
        //If the given flag is -h, give a help message
        case 'v':
            verbose = true;
            break;

        // an unknown argument is supplied
        case '?':
            std::cout << "Invalid option supplied. Terminating." << std::endl;
            cleanUp();
            break;
        }
    }
    
    //Get the perror header
    std::ifstream("/proc/self/comm") >> exe;
    exe.append(": Error");

    //capture sigint 
    signal(SIGINT, siginthandler);

    //stream holders
    std::string currentCommand;

    //forking variables
    pid_t child_pid, wpid;
    int max_run_time = 30;
    int status = 0;

    //check sharedmem
    if(shmid == -1 || Turn_shmid == -1 || Allocated_shmid == -1 || PIDSVec_shmid == -1){
        std::cout << "Shared Memory Error!\n";
        perror(&exe[0]);
    }

    //initialize the PCB queue
    if(msgsnd(PCB_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        std::cout << "Message Failed to send!\n";
        perror(&exe[0]);
    }

    //initialize the Resource queue
    if(msgsnd(resource_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        std::cout << "Message Failed to send!\n";
        perror(&exe[0]);
    }

    //initialize the Maximum queue
    if(msgsnd(maximum_msgid, &msg, strlen(msg.text)+1, 0) == -1){
        perror(&exe[0]);
    }

    //make our time-watching thread and logging start-time
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::thread timeWatcher (threadKill, start, max_run_time);

    //childwatcher
    std::thread childrenWatcher (threadReturn);

    //call parent function
    handleRequests(&child_pid, &wpid, &status);

    //print activity notification
    std::cout << "Done!" << std::endl;

    //clean up sharedmemory, make sure no zombies are present, and exit
    printStats();
}
