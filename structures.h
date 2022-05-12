#include <string>
#include <vector>
#include <tuple>
#include <random>
#include <iostream>
#include <string.h>
#include <sstream>
#include <iomanip>
#include <algorithm>

//Bakery maximum process amount
#define PROCESS_COUNT 20

//Maximum time to run the program
#define MAX_RUN_TIME 100

//Maximum time to allow child creation
#define MAX_INITIALIZATION_TIME 5

//Maximum child processes to allow
#define MAX_PROCESSES 40

// Number of processes
const int P = 18;
  
// Number of resources
const int R = 20;

//Message Queue Struct
struct message{
    long identifier =1;
    char text[200];
};

//Clock Object 
struct Clock
{
    private:
       unsigned int seconds;
       unsigned int nanoseconds;

    public:

        /***********************************************************************
        Function: Clock->getTimeStamp()

        Description: Function to get a Time Stamp at the requested
                     point in time.
        ***********************************************************************/
        std::string getTimeStamp(){
            return (std::to_string(seconds) + ":" + std::to_string(nanoseconds));
        }

        /***********************************************************************
        Function: Clock->getFractionalStamp()

        Description: Function to get the clock as a measurement of only the 
                     ellapsed factional seconds
        ***********************************************************************/
        double getFractionalStamp(){
            return (seconds + (nanoseconds/1000000000));
        }

        /***********************************************************************
        Function: Clock->setTimeStamp(unsigned int nanoseconds)

        Description: Function to set a Time Stamp at the requested
                     point in time.
        ***********************************************************************/
        void setTimeStamp(unsigned int newnanoseconds){

            //add the nanoseconds
            nanoseconds += newnanoseconds;

            //rollover into seconds
            if(nanoseconds >= 1000000000){
                seconds += 1;
                nanoseconds -= 1000000000;
            }
        }
};

//Semaphore for Turn Taking
struct TurnSemaphore{

    private:
        int turn =0;
        int kill =-1;

    public:
        /***********************************************************************
        Function: TurnSemaphore->setTurn(int)

        Description: Function to set which child's turn it is
        ***********************************************************************/
        void setTurn(int PID){

            //update the semaphore
            turn = PID;
        }

        /***********************************************************************
        Function: TurnSemaphore->setKill(int)

        Description: Function to set which child's turn to exit it is
        ***********************************************************************/
        void setKill(int PID){

            //update the semaphore
            kill = PID;
        }

        /***********************************************************************
        Function: TurnSemaphore->getKill(int)

        Description: Function to get which child's turn to exit it is
        ***********************************************************************/
        bool getKill(int PID){
            //return true if PID matches turn
            return (PID == kill);
        }

        /***********************************************************************
        Function: TurnSemaphore->returnTurn(int)

        Description: Function to set turn to 0
        ***********************************************************************/
        void returnTurn(){
            turn = 0;
        }

        /***********************************************************************
        Function: TurnSemaphore->getTurn(int)

        Description: Function to set which child's turn it is
        ***********************************************************************/
        bool getTurn(int PID){
            //return true if PID matches turn
            return (PID == turn);
        }
};

//Allocated Resources and Available Resources Matrices
struct ResourceMatrices{
    private:
        //arrays, because vectors are incredibly glitchy in shared memory
        int AllocatedMatrix[P][R] = {{-1}};
        int AvailableMatrix[R];
        int TotalResourcesMatrix[R];
        int MaximumMatrix[P][R] = {{-1}};
        int activeProcessMask[18] = {0};
        int BlockedProcesses[18] = {0};

    public:
        /***********************************************************************
        Function: ResourceMatrices->hasResources(int)

        Description: Function to check if a process is holding any resources in
                     the tables
        ***********************************************************************/
        bool hasResources(int process){
            
            //check and see if this process has any held resources
            for(int i =0; i < 20; i++){
                if(AllocatedMatrix[process][i] != 0){
                    return true;
                }
            }

            //if not, return false
            return false;
        }

        /***********************************************************************
        Function: ResourceMatrices->returnResource(int)

        Description: Function to return a resource to the free table when signaled
                     to do so from OSS's corresponding 'Safe' function call.
        ***********************************************************************/
        void returnResource(int resource, int amount, int process){

            //Allocate one more of resource to system
            AllocatedMatrix[process][resource] -= 1;

            //remove this same resource from allocated matrix
            AvailableMatrix[resource] += 1;
        }

        /***********************************************************************
        Function: ResourceMatrices->UnblockProcess(int)

        Description: Function to unblock a process and grant it's request
                     when signaled to do so from OSS's corresponding 'Safe' 
                     function call.
        ***********************************************************************/
        std::string UnblockProcess(int process){
            
            //int to hold resource val
            int resource = 0;

            //remove from blocked queue
            resource = BlockedProcesses[process];
            BlockedProcesses[process] = -1;

            //allocate the request it had to it
            allocateResource(resource, 1, process);

            //return the resource and process stuff
            std::stringstream ss;
            ss << "ResourceManager has unblocked Process " << process << " and granted request R" << resource << " at time zzz "<< std::endl;
            return ss.str();
        }

        /***********************************************************************
        Function: ResourceMatrices->TerminateProcess(int)

        Description: Function to terminate a process and harvest it's resources
                     when signaled to do so from OSS's corresponding 'Safe' 
                     function call.
        ***********************************************************************/
        std::string TerminateProcess(int process){
            
            //StringStream operator to handle which resources
            //got returned
            std::stringstream rs;
            int temp;

            //remove from blocked queue
            BlockedProcesses[process] = -1;

            //allocate the request it had to it
            std::string reresources = ReturnResources(process);

            //return the resource and process stuff
            std::stringstream ss;
            ss << "ResourceManager has Terminated Process " << process << "\n\tResources released: ";
            rs << reresources;
            while(rs >> temp){
                ss << "R" << temp << ":1 ";
            }
            ss << std::endl;
            return ss.str();
        }

        /***********************************************************************
        Function: ResourceMatrices->ReturnResources(int)

        Description: Function to move resources from allocated to free
                     when signaled to do so from OSS's corresponding 'Safe' 
                     function call.
        ***********************************************************************/
        std::string ReturnResources(int process){

            //StringStream operator to handle which resources
            //got returned
            std::stringstream ss;

            //Allocate one more of resource back to system
            for(int i =0; i < 20; i++){
                if(AllocatedMatrix[process][i] != 0){
                    int tmp = AllocatedMatrix[process][i];
                    AvailableMatrix[i] += tmp;

                    //add to log
                    ss << i << " ";
                }
                AllocatedMatrix[process][i] = 0;
            }

            //also clean up entry in max matrix
            for(int i =0; i < 20; i++){
                MaximumMatrix[process][i] = 0;
            }

            activeProcessMask[process] = 0;

            return ss.str();
        }

        /***********************************************************************
        Function: ResourceMatrices->allocateResource(int, int, int)

        Description: Function to allocate a resource from the free array to the 
                     allocated matrix when requested by OSS's corresponding 
                     'Safe' call
        ***********************************************************************/
        void allocateResource(int resource, int amount, int process){
            
            //Allocate one more of resource to process
            AllocatedMatrix[process][resource] += 1;

            //remove this same resource from available vector
            AvailableMatrix[resource] -= 1;
        }

        /***********************************************************************
        Function: ResourceMatrices->instantiateMatrix(int, int)

        Description: Function to setup these resources to begin with
        ***********************************************************************/
        void instantiateMatrix(int resourceCount, int processCount){
            //more modern use of C++11
            //random seeds and generators
            std::random_device rd;
            std::mt19937 mt(rd());
            std::uniform_real_distribution<double> resourceAmount(0.0, 10.0);

            //randomly generate resource value matrice
            for(int i=0; i < resourceCount; i++){
                AvailableMatrix[i] = (int)(resourceAmount(mt));
                activeProcessMask[i] = 0;
            }

            //fill the Maxresource with 0
            for(int i =0; i < P; i++){
                for(int j =0; j < R; j++){
                    MaximumMatrix[i][j] = 0;
                }
            }

            //fill the Allocatedresource and blocked processes with 0
            for(int i =0; i < P; i++){
                BlockedProcesses[i] = -1;
                for(int j =0; j < R; j++){
                    AllocatedMatrix[i][j] = 0;
                }
            }

            //fill the total resource matrix
            memcpy (TotalResourcesMatrix, AvailableMatrix, R*sizeof(float));
        }

        /***********************************************************************
        Function: ResourceMatrices->setupEntryinAlloc(int)

        Description: Function to claim a table entry for a new child in allocated
        ***********************************************************************/
        void setupEntryinAlloc(int processID){
            //fill the entry
            for(int i =0; i < 20; i++)
                AllocatedMatrix[processID][i] =0;
        }

        /***********************************************************************
        Function: ResourceMatrices->setupEntry(std::vector<int>, int)

        Description: Function to claim a table entry for a new child in max claims
        ***********************************************************************/
        void setupEntryinMax(std::vector<int> resources, int processID){
            //fill the entry
            for(int i =0; i < 20; i++)
                MaximumMatrix[processID][i] = resources[i];
        }

        /***********************************************************************
        Function: ResourceMatrices->show()

        Description: Function to put the allocated resource table into a string
                     stream so that OSS can use it for logging purposes if
                     the verbose option is set.
        ***********************************************************************/
        std::string show(){

            //string stream operator
            std::stringstream ss;

            //make stringstream
            for(int i =0; i < 18 ;i++)
            {
                for(int j =0; j < 20 ;j++)
                {
                    ss << AllocatedMatrix[i][j] << " ";
                }
                ss << std::endl;
            }

            return ss.str();
        }

        /***********************************************************************
        Function: ResourceMatrices->isSafe(int[], int[], int[])

        Description: Function to check through use of a safety algorithm whether or
                     not a particular configuration leads to an unsafe state
        ***********************************************************************/
        bool isSafe(int avail[20], int maxm[18][20], int alloc[18][20]){

            //working arrays to manipulate data
            int finish[18],need[18][20];
            
            //temporary vectors for manipulating data
            int dead[18];
            int safe[18];

            //ints for paramter storing and temp values
            int n = 18, r = 20, temp, flag=1, k, c1=0;
            
            //fill the finished matrix with false
            for(int i=0; i<18; i++){
                finish[i]=0;
            }
            
            //Calculate the need matrix
            for(int i = 0; i < 18; i++){
                for(int j = 0; j < 20; j++){
                    need[i][j]=maxm[i][j]-alloc[i][j];
                }
            }
            
            //matrix reduction
            while(flag){
                flag=0;
                for(int i = 0; i < 18; i++){
                    int c=0;
                    for(int j =0; j < r; j++){
                        if((finish[i] == 0) && (need[i][j] <= avail[j])){
                            c++;
                            if(c == r){
                                for(int k=0; k<r; k++){
                                    avail[k]+=alloc[i][j];
                                    finish[i]=1;
                                    flag=1;
                                }
                                i = (n)*(finish[i] == 1) + (i)*(finish[i] != 1);
                            }
                        }
                    }
                }
            }

            //checking which processes actually managed to finish
            int j = 0;
            flag = 0;
            for(int i=0; i<n; i++){
                if(finish[i] == 0){
                    dead[j] = i;
                    j++;
                    flag = 1;
                }
            }

            //checking results
            if(flag == 1 ){
                return false;
            }
            else{
                return true;
            }  
        }

        /***********************************************************************
        Function: ResourceMatrices->StartSafetyAlgorithm(int, int, int)

        Description: Function to make the temporary arrays and get everything
                     ready for running the safety algorithm to determine if a 
                     particular system configuration is safe or not. It's called
                     when OSS's corresponding 'Safe' function is called.
        ***********************************************************************/
        std::string StartSafetyAlgorithm(int resource, int resourceAmountRequested, int processID){

            //Set up our matrices to use in the banker's algorithm
            int SimulatedAllocatedMatrix[P][R];
            memcpy (SimulatedAllocatedMatrix, AllocatedMatrix, P*R*sizeof(float));
            int SimulatedAvailableMatrix[R];
            memcpy (SimulatedAvailableMatrix, AvailableMatrix, R*sizeof(float));
            int SimulatedMaximumMatrix[P][R];
            memcpy (SimulatedMaximumMatrix, MaximumMatrix, P*R*sizeof(float));

            //update the matrix to reflect the state of the system if we were to allocate this resource
            SimulatedAllocatedMatrix[processID][resource] += resourceAmountRequested;
            SimulatedAvailableMatrix[resource] -= resourceAmountRequested;

            //string stream object
            std::stringstream ss;

            //output the request (currently in cout FIXME: CHANGE TO FILE FIN)
            ss << "ResourceManager has detected Process " << processID << " requesting R" << resource << " at time xxx "<< std::endl;
            ss << "ResourceManager running deadlock avoidance at this time" <<std::endl;

            //check with our safety algorithm (banker's) and allocate if it's safe to do so
            if(isSafe(SimulatedAvailableMatrix, SimulatedMaximumMatrix, SimulatedAllocatedMatrix)){

                //allocate the desired resource
                allocateResource(resource, resourceAmountRequested, processID);
                ss << "\tSafe state after running Safety Detection algorithm" << std::endl;
                ss << "\tResourceManager granting Process " << processID << " request R" << resource << " at time yyy" << std::endl;
                return ss.str();
            }
            else{
                //deny the request and log it and add to blocked queue
                BlockedProcesses[processID] =  resource;
                ss << "\tUnsafe state after running Safety Detection algorithm" << std::endl;
                ss << "\tResourceManager not granting Process " << processID << " request R" << resource << " at time yyy" << std::endl;
                return ss.str();
            }
        }

        /***********************************************************************
        Function: ResourceMatrices->startBlockedCheck()

        Description: Function to check the blocked queue for any processes that
                     can be unblocked. If there are any that can be, it returns 
                     their PIDS for OSS to evaluate. It's called when OSS's 
                     corresponding 'Safe' function call is made.
        ***********************************************************************/
        std::string startBlockedCheck(){
            //check blocked queue and make list of proposed
            //unblocks to relay back to OSS

            //sstream for holding return string
            std::stringstream ss;
            ss << " ";

            for(int i=0; i < 18; i++){   
                //check each blocked process to see if it can unblock
                if(BlockedProcesses[i] != -1){
                    if(AvailableMatrix[BlockedProcesses[i]] > 0){
                        
                        //refresh our matrices so that we are always working with the real system for our scenarios
                        int SimulatedAllocatedMatrix[P][R];
                        memcpy (SimulatedAllocatedMatrix, AllocatedMatrix, P*R*sizeof(float));
                        int SimulatedAvailableMatrix[R];
                        memcpy (SimulatedAvailableMatrix, AvailableMatrix, R*sizeof(float));
                        int SimulatedMaximumMatrix[P][R];
                        memcpy (SimulatedMaximumMatrix, MaximumMatrix, P*R*sizeof(float));

                        //update the matrix to reflect the state of the system if we were to allocate this resource
                        SimulatedAllocatedMatrix[i][BlockedProcesses[i]] += 1;
                        SimulatedAvailableMatrix[BlockedProcesses[i]] -= 1;

                        //check to see if this is a safe move
                        if(isSafe(SimulatedAvailableMatrix, SimulatedMaximumMatrix, SimulatedAllocatedMatrix)){
                            
                            //Push proposed unblock to stream operator
                            //std::cout << "Added process to proposed unblock" << std::endl;
                            ss << i << " ";

                        }
                    }
                }
            }
            //std::cout << ss.str();
            return ss.str();
        }

        /***********************************************************************
        Function: ResourceMatrices->StartDeadlockRecovery()

        Description: Function to handle a deadlock in the blocked queues. If a 
                     process can be killed to free another, then we do it. Otherwise,
                     we kill indescriminately based on FIFO in the blocked queue.
                     This function is called by OSS's corresponding 'Safe' call.
        ***********************************************************************/
        std::string StartDeadlockRecovery(){
            //we're deadlocked. Time to handle it.
            //the execution method will check the desired
            //resources of the processes in the queue in LIFO
            //priority. If a process can be granted it's request
            //by killing another process, it is done.

            //bitmask in string for holding our fallen processes
            //sstream for holding return string
            std::stringstream ss;
            std::vector<int> holdPids;
            ss << " ";

            //determine which processes need to be culled to allow others to progress
            for(int i=0; i < 18; i++){   
                if(BlockedProcesses[i] != -1){
                    //only continue if this process isn't marked for death (Might add back later)
                    if(1) {

                        //loop over all elements in blocked
                        for(int j =0; j < 18; j++){
                            if(BlockedProcesses[j] != -1){

                                //if process can be terminated to secure a desired resource and free the deadlock, do it.
                                if(AllocatedMatrix[j][BlockedProcesses[i]] > 0){

                                    holdPids.push_back(j);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            if(!holdPids.empty()){
                //make sure we have no duplicates (I've become paranoid after hours of bugs)
                std::sort(holdPids.begin(), holdPids.end()); 
                auto last = std::unique(holdPids.begin(), holdPids.end());
                holdPids.erase(last, holdPids.end());

                //convert to stream
                for(int i =0; i < holdPids.size(); i++){
                    //std::cout << "Added process " << holdPids[i] << " to proposed Terminate" << std::endl;
                    ss << holdPids[i] << " ";
                }

                //std::cout << ss.str();
                return ss.str();
            }
            else{

                //if no immediate overlap is found, Preempt some processes
                int count = 0;
                for(int i=0; i < 18; i++){   
                    if(BlockedProcesses[i] != -1){
                        count++;
                        holdPids.push_back(i);
                    }
                    if(count > 1)
                        break;
                }

                //convert to stream
                for(int i =0; i < holdPids.size(); i++){
                    //std::cout << "Added process " << holdPids[i] << " to proposed Terminate" << std::endl;
                    ss << holdPids[i] << " ";
                }

                //std::cout << ss.str();
                return ss.str();
            }
        }

        /***********************************************************************
        Function: ResourceMatrices->getProcessIDandSetArrays()

        Description: Function to get a child process it's 'PID' or table entry
                     so that it knows which resource entry it takes up. It then 
                     fills the max and allocated entries up so the process can
                     use it.
        ***********************************************************************/
        int getProcessIDandSetArrays(){

            //hold pid temp
            int ProcessID;

            //temp vector
            std::vector<int> maximum;

            //random number C++ gen
            std::random_device rd;
            std::mt19937 mt(rd());

            //make sure we don't set our max at a number larger than possible allocations
            for(int i=0; i < 20; i++){
                std::uniform_real_distribution<double> resourceRange(0.0, (TotalResourcesMatrix[i]-1.0));
                maximum.push_back(resourceRange(mt));
            }

            //get our PID out of the maximum resource matrix by doing some math
            for(int i=0; i < 18; i++){

                //starting variable
                int mult = 0;
                for(int j =0; j < 20; j++){
                    //add all elements in this row of the array
                    mult += MaximumMatrix[i][j];
                }

                //if mult still equals 0, then all elements in that row are empty, and the slot is available
                if(mult == 0){
                    ProcessID = i;
                    activeProcessMask[ProcessID] = 1;
                    break;
                }
            }

            //set allocation vector entry 
            setupEntryinAlloc(ProcessID);

            //set max vector entry 
            setupEntryinMax(maximum, ProcessID);

            return ProcessID;
        }

        /*************************************************************************
        Function: ResourceMatrices->FormulateReturn(int)

        Description: Function to get a child's table for it to let it know which
                     resoruce it can return.
        *************************************************************************/
        int FormulateReturn(int ProcessID){

            //build simulated matrices
            int SimulatedAllocatedMatrix[P][R];
            memcpy (SimulatedAllocatedMatrix, AllocatedMatrix, P*R*sizeof(float));
            int SimulatedAvailableMatrix[R];
            memcpy (SimulatedAvailableMatrix, AvailableMatrix, R*sizeof(float));
            int SimulatedMaximumMatrix[P][R];
            memcpy (SimulatedMaximumMatrix, MaximumMatrix, P*R*sizeof(float));

            //get the row that corresponds with the PID given
            int MaximumVector[20];
            // set the contents of MaximumVector equal to the contents of the first row of MaximumMatrix.
            memcpy(MaximumVector, MaximumMatrix[ProcessID], sizeof(MaximumVector)); 
            //get the row that corresponds with the PID given
            int AllocatedVector[20];
            // set the contents of MaximumVector equal to the contents of the first row of MaximumMatrix.
            memcpy(AllocatedVector, AllocatedMatrix[ProcessID], sizeof(AllocatedVector)); 

            for(int i =0; i < 20; i++){
                if (AllocatedMatrix[ProcessID][i] != 0){
                    //only return a resource when we have one to return
                    AllocatedMatrix[ProcessID][i] -= 1;

                    //remove this same resource from available vector
                    AvailableMatrix[i] += 1;

                    //let OSS know what happened here and let it finish the job by logging it
                    return i;
                }
            }

            //if we don't have any resources to return 
            return -1;
        }

        /*************************************************************************
        Function: ResourceMatrices->FormulateRequest(int)

        Description: Function to check the possible resources to make sure a 
                     child doesn't request more of a resource than the system has in
                     max
        *************************************************************************/
        int FormulateRequest(int ProcessID){
            //build our request
            //get the allocated
            int SimulatedAllocatedMatrix[P][R];
            memcpy (SimulatedAllocatedMatrix, AllocatedMatrix, P*R*sizeof(float));
            int SimulatedAvailableMatrix[R];
            memcpy (SimulatedAvailableMatrix, AvailableMatrix, R*sizeof(float));
            int SimulatedMaximumMatrix[P][R];
            memcpy (SimulatedMaximumMatrix, MaximumMatrix, P*R*sizeof(float));

            //get the row that corresponds with the PID given
            int MaximumVector[20];
            // set the contents of MaximumVector equal to the contents of the first row of MaximumMatrix.
            memcpy(MaximumVector, MaximumMatrix[ProcessID], sizeof(MaximumVector)); 
            //get the row that corresponds with the PID given
            int AllocatedVector[20];
            // set the contents of MaximumVector equal to the contents of the first row of MaximumMatrix.
            memcpy(AllocatedVector, AllocatedMatrix[ProcessID], sizeof(AllocatedVector)); 

            //more modern use of C++11
            //random seeds and generators
            std::random_device rd;
            std::mt19937 mt(rd());
            std::uniform_real_distribution<double> resource(0.0, 20.0);

            //subtract allocated from max to get possible requests
            int reschoice;
            bool gotone = false;
            while(!gotone){
                reschoice = resource(mt);
                if(MaximumVector[reschoice] - AllocatedVector[reschoice] > 0)
                    gotone = true;
            }

            //get our desired resource 
            return reschoice;
        }
};

//vector for storing PIDS
struct PIDSVector{
    public:
        std::vector<std::tuple<int,int>> PIDS;

        /***********************************************************************
        Function: PIDSVector->getMask()

        Description: Function to get a pid mask value from the vector
        ***********************************************************************/
        int getMask(int PID){
            //find the desired PID mask and return it
            for(int i=0; i < PIDS.size(); i++){
                if(std::get<0>(PIDS[i]) == PID){
                    return std::get<1>(PIDS[i]);
                }                
            }
        }
};

//Semaphore for SIGINT Handling
struct SigSemaphore{

    private:
        bool kill = false;
        bool received = false;

    public:
        /***********************************************************************
        Function: SigSemaphore->signal()

        Description: Function to set whether signint has been passed
        ***********************************************************************/
        void signal(){

            //update the semaphore
            kill = true;
        }

        /***********************************************************************
        Function: SigSemaphore->signalhandshake()

        Description: Function to set whether signint has been passed
        ***********************************************************************/
        void signalhandshake(){

            //update the semaphore
            received = true;
        }

        /***********************************************************************
        Function: SigSemaphore->check()

        Description: Function to get whether signint has been passed
        ***********************************************************************/
        bool check(){
            return kill;
        }

        /***********************************************************************
        Function: SigSemaphore->handshake()

        Description: Function to get whether signint has been passed
        ***********************************************************************/
        bool handshake(){

            return received;
        }
};