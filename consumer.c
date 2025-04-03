/*
CSC139 
First Assignment
Delemos, Jonathon 
Section #1
OSs Tested on: Ubuntu Linux
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h> // For sleep()

// Size of shared memory block
// Pass this to ftruncate and mmap
#define SHM_SIZE 4096

// Global pointer to the shared memory block
// This should receive the return value of mmap
// Don't change this pointer in any function
void* gShmPtr;

// You won't necessarily need all the functions below
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

int main()
{
    const char *name = "OS_HW1_JonathonDelemos"; // Name of shared memory block
    int shm_fd; // Shared memory file descriptor
    int bufSize; // Bounded buffer size
    int itemCnt; // Number of items to consume
    int in; // Index of next item to produce
    int out; // Index of next item to consume

    // **Wait for producer to create shared memory**
    sleep(1);

    // **Attempt to open shared memory with retries**
    int attempts = 5;
    printf("Opening shared memory with name: %s\n", name);

    while ((shm_fd = shm_open(name, O_RDWR, 0666)) == -1 && attempts-- > 0) {
        printf("Shared memory not available, retrying...\n");
        sleep(1);
    }
    if (shm_fd == -1) {
        printf("Shared memory failed to open after retries\n");
        exit(1);
    }

    // Write code here to consume all the items produced by the producer
    // Use the functions provided below to get/set the values of shared variables in, out, bufSize
    // Use the provided function ReadAtBufIndex() to read from the bounded buffer 	
    // **Extremely Important: Remember to set the value of any shared variable you change locally
    // Use the following print statement to report the consumption of an item:
    // printf("Consuming Item %d with value %d at Index %d\n", i, val, out);

    // **Map shared memory for both reading and writing**
    gShmPtr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (gShmPtr == MAP_FAILED) {
        printf("Mapping failed\n");
        exit(1);
    }

    // **Print actual memory values immediately after mapping**
    printf("Immediately after mapping, consumer sees: bufSize = %d, itemCnt = %d, in = %d, out = %d\n",
           GetBufSize(), GetItemCnt(), GetIn(), GetOut());

    // **Read values from shared memory after waiting**
    bufSize = GetBufSize();
    itemCnt = GetItemCnt();
    in = GetIn();
    out = GetOut();

    // **Confirm correct values are read**
    printf("Consumer reading AFTER WAIT: bufSize = %d, itemCnt = %d, in = %d, out = %d\n",
           bufSize, itemCnt, in, out);

    // **Consume all items produced by the producer**
    for (int i = 0; i < itemCnt; i++) {
        // **Wait until there is an item to consume**
        while (GetIn() == out) {
            if (i >= itemCnt - 1) break; // Stop if all items are consumed
        }

        // **Read item from shared memory buffer**
        int val = ReadAtBufIndex(out);
        printf("Consuming Item %d with value %d at Index %d\n", i, val, out);

        // **Update 'out' index and write it back to shared memory**
        out = (out + 1) % bufSize;
        SetOut(out);
    }

    // **Unmap memory but do NOT unlink (producer should handle this)**
    if (munmap(gShmPtr, SHM_SIZE) == -1) {
        printf("Error unmapping memory\n");
    }

    return 0;
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
    void* ptr = gShmPtr + i * sizeof(int);
    memcpy(&val, ptr, sizeof(int));
    return val;
}

// Set the ith value in the header
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
    void* ptr = gShmPtr + 4 * sizeof(int) + indx * sizeof(int);
    memcpy(ptr, &val, sizeof(int));
}

// Read the val at the given index in the bounded buffer
int ReadAtBufIndex(int indx)
{
    int val;
    void* ptr = gShmPtr + 4 * sizeof(int) + indx * sizeof(int);
    memcpy(&val, ptr, sizeof(int));
    return val;
}
