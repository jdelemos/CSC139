/*
CSC139 
First Assignment
Delemos, Jonathon 
Section: 139-01
OSs Tested on: Ubuntu Linux
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


// Size of shared memory block
// Pass this to ftruncate and mmap
#define SHM_SIZE 4096

// Global pointer to the shared memory block
// This should receive the return value of mmap
// Don't change this pointer in any function
void* gShmPtr;

// You won't necessarily need all the functions below
void Producer(int, int, int);
void InitShm(int, int);
void SetBufSize(int);
void SetItemCnt(int);
void SetIn(int);
void SetOut(int);
void SetHeaderVal(int, int);
int GetBufSize();
int GetItemCnt();
int GetIn();
int GetOut();
int GetHeaderVal(int);
void WriteAtBufIndex(int, int);
int ReadAtBufIndex(int);
int GetRand(int, int);


int main(int argc, char* argv[])
{
        pid_t pid;
        int bufSize; // Bounded buffer size
        int itemCnt; // Number of items to be produced
        int randSeed; // Seed for the random number generator 

        if(argc != 4){
		printf("Invalid number of command-line arguments\n");
		exit(1);
        }
	bufSize = atoi(argv[1]);
	itemCnt = atoi(argv[2]);
	randSeed = atoi(argv[3]);
	
	// Write code to check the validity of the command-line arguments
        if (bufSize < 2 || bufSize >450) {
                printf("Invalid command line argument: Buffer size does not fall within correct range."); 
                exit(1); 
        } 
        if (itemCnt < 0) {
                printf("Invalid command line argument: Item count is not large enough."); 
                exit(1); 
        }
        // Function that creates a shared memory segment and initializes its header
        InitShm(bufSize, itemCnt);        

	/* fork a child process */ 
	pid = fork();

	if (pid < 0) { /* error occurred */
		fprintf(stderr, "Fork Failed\n");
		exit(1);
	}
	else if (pid == 0) { /* child process */
		printf("Launching Consumer \n");
		execlp("./consumer","consumer",NULL);
	}
	else { /* parent process */
		/* parent will wait for the child to complete */
		printf("Starting Producer\n");
		
               // The function that actually implements the production
               Producer(bufSize, itemCnt, randSeed);
		
	       printf("Producer done and waiting for consumer\n");
	       wait(NULL);		
	       printf("Consumer Completed\n");
        }
    
        return 0;
}

void InitShm(int bufSize, int itemCnt)
{          
    const char *name = "OS_HW1_JonathonDelemos"; 
    int fd; 

    printf("Opening shared memory with name: %s\n", name);

    // Create shared memory
    fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open failed");
        exit(1);
    }

    // Set size of shared memory
    if (ftruncate(fd, SHM_SIZE) == -1) {
        perror("ftruncate failed");
        exit(1);
    }

    gShmPtr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    
    if (gShmPtr == NULL) {
    printf("Error: gShmPtr is NULL before writing to shared memory!\n");
    exit(1);
    }


    if (gShmPtr == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    // **WRITE VALUES TO SHARED MEMORY**
    SetHeaderVal(0, bufSize);  
    printf("After SetHeaderVal(0, %d), Read Back: %d\n", bufSize, GetBufSize());

    SetHeaderVal(1, itemCnt);  
    printf("After SetHeaderVal(1, %d), Read Back: %d\n", itemCnt, GetItemCnt());

    SetHeaderVal(2, 0);        
    printf("After SetHeaderVal(2, 0), Read Back: %d\n", GetIn());

    SetHeaderVal(3, 0);        
    printf("After SetHeaderVal(3, 0), Read Back: %d\n", GetOut());

    // **PRINT ACTUAL MEMORY VALUES AFTER WRITING**
    printf("After writing, memory contains: bufSize = %d, itemCnt = %d, in = %d, out = %d\n",
           GetBufSize(), GetItemCnt(), GetIn(), GetOut());
}




void Producer(int bufSize, int itemCnt, int randSeed)
{
    int in = GetIn();  // Initialize in from shared memory
    int out = GetOut(); // Get initial out value

    srand(randSeed);

    for (int i = 0; i < itemCnt; i++)
    {
        int val = GetRand(2, 3200); // Generate a random number in the specified range

       // Write code here to produce itemCnt integer values in the range specificed in the problem description
       // Use the functions provided below to get/set the values of shared variables "in" and "out"
       // Use the provided function WriteAtBufIndex() to write into the bounded buffer 	
       // Use the provided function GetRand() to generate a random number in the specified range
       // **Extremely Important: Remember to set the value of any shared variable you change locally
       // Use the following print statement to report the production of an item:
       // printf("Producing Item %d with value %d at Index %d\n", i, val, in);
       // where i is the item number, val is the item value, in is its index in the bounded buffer


        // Wait if the buffer is full (next in == out)
        while (((in + 1) % bufSize) == out) {
            out = GetOut(); // Continuously update 'out' while waiting
        }

        // Write value into shared buffer at index 'in'
        WriteAtBufIndex(in, val);

        // Print production message
        printf("Producing Item %d with value %d at Index %d\n", i, val, in);

        // Move 'in' forward
        in = (in + 1) % bufSize;

        // Update shared memory with new 'in' value
        SetIn(in);
    }

    printf("Producer Completed\n");
}


// Set the value of shared variable "bufSize"
void SetBufSize(int val)
{
        SetHeaderVal(0, val);
}

// Set the value of shared variable "itemCnt"
void SetItemCnt(int val)
{
        SetHeaderVal(1, val);
}

// Set the value of shared variable "in"
void SetIn(int val)
{
        SetHeaderVal(2, val);
}

// Set the value of shared variable "out"
void SetOut(int val)
{
        SetHeaderVal(3, val);
}

// Get the ith value in the header
int GetHeaderVal(int i)
{
        int val;
        void* ptr = gShmPtr + i*sizeof(int);
        memcpy(&val, ptr, sizeof(int));
        return val;
}

void SetHeaderVal(int i, int val)
{
    void* ptr = gShmPtr + i * sizeof(int);

    memcpy(ptr, &val, sizeof(int));

}


// Get the value of shared variable "bufSize"
int GetBufSize()
{       
        return GetHeaderVal(0);
}

// Get the value of shared variable "itemCnt"
int GetItemCnt()
{
        return GetHeaderVal(1);
}

// Get the value of shared variable "in"
int GetIn()
{
        return GetHeaderVal(2);
}

// Get the value of shared variable "out"
int GetOut()
{             
        return GetHeaderVal(3);
}


// Write the given val at the given index in the bounded buffer 
void WriteAtBufIndex(int indx, int val)
{
        // Skip the four-integer header and go to the given index 
        void* ptr = gShmPtr + 4*sizeof(int) + indx*sizeof(int);
	memcpy(ptr, &val, sizeof(int));
}

// Read the val at the given index in the bounded buffer
int ReadAtBufIndex(int indx)
{
        // Write the implementation
 
}

// Get a random number in the range [x, y]
int GetRand(int x, int y)
{
	int r = rand();
	r = x + r % (y-x+1);
        return r;
}
