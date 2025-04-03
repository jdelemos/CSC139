
/*
 * MTFindProd.c
 *
 * Author: Jonathon Deleoms
 * Section: 01
 * Tested on: Linux (Ubuntu-24.04) with GCC (9.3.0)
 * Hardware: ThinkPad 490S (Intel i7-8560U, 24GB RAM)
 *
 * CSC139: Operating System Principles - Second Assignment
 *
 * This program generates an array of random numbers and computes the modular
 * product of its elements using several schemes:
 *  1. Sequential multiplication.
 *  2. Threaded multiplication with the parent waiting for all children.
 *  3. Threaded multiplication with the parent continually checking on children.
 *  4. Threaded multiplication with the parent waiting on a semaphore.
 *
 * Compile with:
 *    gcc -O3 MTFindProd.c -o MTFindProd -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <semaphore.h>
#include <stdbool.h>
#include <sched.h>  // for sched_yield
#include <unistd.h> // for usleep
#include <stdatomic.h>

#define MAX_SIZE 100000000
#define MAX_THREADS 16
#define RANDOM_SEED 7649
#define MAX_RANDOM_NUMBER 3000
#define NUM_LIMIT 9973

// Global variables
volatile long gRefTime;       // For timing
volatile int gData[MAX_SIZE]; // The array that will hold the data

volatile int gThreadCount;              // Number of threads
volatile int gDoneThreadCount;          // Number of threads that are done at a certain point. Whenever a thread is done, it increments this. Used with the semaphore-based solution
volatile int gThreadProd[MAX_THREADS];  // The modular product for each array division that a single thread is responsible for
volatile bool gThreadDone[MAX_THREADS]; // Is this thread done? Used when the parent is continually checking on child threads
// Modify the declaration of found_zero
volatile atomic_bool found_zero = false; // Shared flag indicating if zero is found

// Semaphores
sem_t completed;              // To notify parent that all threads have completed or one of them found a zero
sem_t mutex;                  // Binary semaphore to protect the shared variable gDoneThreadCount
pthread_attr_t *thread_attrs; // Array to store thread attributes

int SqFindProd(int size);                   // Sequential FindProduct (no threads) computes the product of all the elements in the array mod NUM_LIMIT
void *ThFindProd(void *param);              // Thread FindProduct but without semaphores
void *ThFindProdWithSemaphore(void *param); // Thread FindProduct with semaphores
int ComputeTotalProduct();                  // Multiply the division products to compute the total modular product
void InitSharedVars();
void GenerateInput(int size, int indexForZero);                                 // Generate the input array
void CalculateIndices(int arraySize, int thrdCnt, int indices[MAX_THREADS][3]); // Calculate the indices to divide the array into T divisions, one division per thread
int GetRand(int min, int max);                                                  // Get a random number between min and max

volatile void SetTime(void);
volatile long GetTime(void);
volatile long GetCurrentTime(); // Function to get the current time in milliseconds

// Declare global variables and structures
int array_size, num_threads, zero_index; // Input parameters
int final_product = 1;                   // Global final product

pthread_t *threads;              // Array to store thread IDs
sem_t semaphore;                 // Semaphore for synchronization
pthread_mutex_t lock;            // Mutex for protecting shared variables
volatile int busy_wait_flag = 0; // Flag for busy-waiting scheme

// Struct to store thread-specific data
typedef struct
{
    int id;     // Thread ID
    int start;  // Start index of the assigned array segment
    int end;    // End index of the assigned array segment
    int result; // Thread's computed product
} ThreadData;

ThreadData *thread_data; // Array to hold thread-specific data
int (*indices)[3];       // 2D array to store thread division indices

// Function declarations
void *ThFindProd(void *arg);                       // Thread function for computing modular product
void initialize_variables(int argc, char *argv[]); // Function to parse input and initialize global variables
void create_threads();                             // Function to create threads
void wait_for_threads();                           // Function to wait for all threads using pthread_join

int main(int argc, char *argv[])
{
    pthread_t tid[MAX_THREADS];
    pthread_attr_t attr[MAX_THREADS];
    int indices[MAX_THREADS][3];
    int i, indexForZero, arraySize, prod;

    // Code for parsing and checking command-line arguments
    if (argc != 4)
    {
        fprintf(stderr, "Invalid number of arguments!\n");
        exit(-1);
    }
    if ((arraySize = atoi(argv[1])) <= 0 || arraySize > MAX_SIZE)
    {
        fprintf(stderr, "Invalid Array Size\n");
        exit(-1);
    }
    gThreadCount = atoi(argv[2]);
    if (gThreadCount > MAX_THREADS || gThreadCount <= 0)
    {
        fprintf(stderr, "Invalid Thread Count\n");
        exit(-1);
    }
    indexForZero = atoi(argv[3]);
    if (indexForZero < -1 || indexForZero >= arraySize)
    {
        fprintf(stderr, "Invalid index for zero!\n");
        exit(-1);
    }

    GenerateInput(arraySize, indexForZero);

    CalculateIndices(arraySize, gThreadCount, indices);

    // Code for the sequential part
    SetTime();
    prod = SqFindProd(arraySize);
    printf("Sequential multiplication completed in %ld ms. Product = %d\n", GetTime(), prod);

    // Threaded with parent waiting for all child threads
    InitSharedVars();
    SetTime();

    // Initialize threads and create them
    pthread_mutex_init(&lock, NULL);
    for (i = 0; i < gThreadCount; i++)
    {
        pthread_attr_init(&attr[i]);
        ThreadData *data = (ThreadData *)malloc(sizeof(ThreadData));
        data->id = i;
        data->start = indices[i][1];
        data->end = indices[i][2];
        data->result = 1;
        pthread_create(&tid[i], &attr[i], ThFindProd, data);
    }

    // Wait for all threads to complete
    for (i = 0; i < gThreadCount; i++)
    {
        pthread_join(tid[i], NULL);
    }

    prod = ComputeTotalProduct();
    printf("Threaded multiplication with parent waiting for all children completed in %ld ms. Product = %d\n", GetTime(), prod);
    // Multi-threaded with busy waiting
    InitSharedVars();
    atomic_store(&found_zero, false);

    SetTime(); // Start timing before creating threads

    // Create threads
    for (i = 0; i < gThreadCount; i++)
    {
        pthread_attr_init(&attr[i]);
        ThreadData *data = (ThreadData *)malloc(sizeof(ThreadData));
        data->id = i;
        data->start = indices[i][1];
        data->end = indices[i][2];
        data->result = 1;
        pthread_create(&tid[i], &attr[i], ThFindProd, data);
    }

    // Busy-wait loop
    while (1)
    {
        pthread_mutex_lock(&lock);
        bool done = (gDoneThreadCount >= gThreadCount || atomic_load(&found_zero));
        pthread_mutex_unlock(&lock);

        if (done)
            break;

        sched_yield();
    }

    // Cancel threads if zero found
    if (atomic_load(&found_zero))
    {
        for (int i = 0; i < gThreadCount; i++)
        {
            pthread_cancel(tid[i]);
        }
    }

    // Compute product
    prod = found_zero ? 0 : ComputeTotalProduct();
    printf("Threaded multiplication with parent continually checking on children completed in %ld ms. Product = %d\n", GetTime(), prod);

    // Multi-threaded with busy waiting (Second Scheme)
    InitSharedVars();
    atomic_store(&found_zero, false);

    SetTime(); // Start timing before thread creation

    // Create threads
    for (i = 0; i < gThreadCount; i++)
    {
        pthread_attr_init(&attr[i]);
        ThreadData *data = (ThreadData *)malloc(sizeof(ThreadData));
        data->id = i;
        data->start = indices[i][1];
        data->end = indices[i][2];
        data->result = 1;
        pthread_create(&tid[i], &attr[i], ThFindProd, data);
    }

    // Busy-wait loop (no semaphores)
    while (1)
    {
        pthread_mutex_lock(&lock);
        bool done = (gDoneThreadCount >= gThreadCount || atomic_load(&found_zero));
        pthread_mutex_unlock(&lock);

        if (done)
            break;

        sched_yield(); // Yield CPU to avoid hogging resources
    }

    // Cancel all threads if zero found
    if (atomic_load(&found_zero))
    {
        for (int i = 0; i < gThreadCount; i++)
        {
            pthread_cancel(tid[i]);
        }
    }

    prod = found_zero ? 0 : ComputeTotalProduct();
    printf("Threaded multiplication with parent continually checking on children completed in %ld ms. Product = %d\n", GetTime(), prod);
}

// Write a regular sequential function to multiply all the elements in gData mod NUM_LIMIT
// REMEMBER TO MOD BY NUM_LIMIT AFTER EACH MULTIPLICATION TO PREVENT YOUR PRODUCT VARIABLE FROM OVERFLOWING
int SqFindProd(int size)
{
    int product = 1;
    for (int i = 0; i < size; i++)
    {
        if (gData[i] == 0)
        {
            return 0; // Terminate early if zero is found
        }
        product = (product * gData[i]) % NUM_LIMIT; // Compute product mod NUM_LIMIT
    }
    return product;
}

// Write a thread function that computes the product of all the elements in one division of the array mod NUM_LIMIT
// REMEMBER TO MOD BY NUM_LIMIT AFTER EACH MULTIPLICATION TO PREVENT YOUR PRODUCT VARIABLE FROM OVERFLOWING
// When it is done, this function should store the product in gThreadProd[threadNum] and set gThreadDone[threadNum] to true
void *ThFindProd(void *param)
{
    ThreadData *data = (ThreadData *)param;
    int product = 1;
    for (int i = data->start; i <= data->end; i++)
    {
        // Check if another thread found a zero (atomic load)
        if (atomic_load(&found_zero))
        {
            pthread_exit(NULL);
        }

        if (gData[i] == 0)
        {
            // Set found_zero atomically
            atomic_store(&found_zero, true);
            pthread_mutex_lock(&lock);
            gThreadProd[data->id] = 0;
            pthread_mutex_unlock(&lock);
            pthread_exit(NULL);
        }
        product = (product * gData[i]) % NUM_LIMIT;
    }

    pthread_mutex_lock(&lock);
    gThreadProd[data->id] = product;
    gDoneThreadCount++;
    pthread_mutex_unlock(&lock);
    pthread_exit(NULL);
}

// Write a thread function that computes the product of all the elements in one division of the array mod NUM_LIMIT
// REMEMBER TO MOD BY NUM_LIMIT AFTER EACH MULTIPLICATION TO PREVENT YOUR PRODUCT VARIABLE FROM OVERFLOWING
// When it is done, this function should store the product in gThreadProd[threadNum]
// If the product value in this division is zero, this function should post the "completed" semaphore
// If the product value in this division is not zero, this function should increment gDoneThreadCount and
// post the "completed" semaphore if it is the last thread to be done
// Don't forget to protect access to gDoneThreadCount with the "mutex" semaphore
void *ThFindProdWithSemaphore(void *param)
{
    ThreadData *data = (ThreadData *)param;
    int product = 1;

    for (int i = data->start; i <= data->end; i++)
    {
        if (gData[i] == 0)
        {
            sem_wait(&mutex);
            gThreadProd[data->id] = 0; // Explicitly set to zero
            found_zero = true;
            sem_post(&semaphore);
            sem_post(&mutex);

            pthread_exit(NULL);
        }
        product = (product * gData[i]) % NUM_LIMIT;
    }

    sem_wait(&mutex);
    gThreadProd[data->id] = product;
    gDoneThreadCount++;
    if (gDoneThreadCount == gThreadCount)
    {
        sem_post(&semaphore); // Notify parent if all threads done
    }
    sem_post(&mutex);

    pthread_exit(NULL);
}

void InitSharedVars()
{
    int i;

    for (i = 0; i < gThreadCount; i++)
    {
        gThreadDone[i] = false;
        gThreadProd[i] = 1;
    }
    gDoneThreadCount = 0;
}

// Write a function that fills the gData array with random numbers between 1 and MAX_RANDOM_NUMBER
// If indexForZero is valid and non-negative, set the value at that index to zero
void GenerateInput(int size, int indexForZero)
{
    srand(RANDOM_SEED); // Set a fixed seed for the random number generator
    for (int i = 0; i < size; i++)
    {
        gData[i] = (rand() % MAX_RANDOM_NUMBER) + 1;
    }
    if (indexForZero >= 0 && indexForZero < size)
    {
        gData[indexForZero] = 0; // Insert zero at the specified index if valid
    }
}

// Write a function that calculates the right indices to divide the array into thrdCnt equal divisions
// For each division i, indices[i][0] should be set to the division number i,
// indices[i][1] should be set to the start index, and indices[i][2] should be set to the end index
void CalculateIndices(int arraySize, int thrdCnt, int indices[MAX_THREADS][3])
{
    int chunk_size = arraySize / thrdCnt;
    for (int i = 0; i < thrdCnt; i++)
    {
        indices[i][0] = i;                                                                       // Store division number
        indices[i][1] = i * chunk_size;                                                          // Start index
        indices[i][2] = (i == thrdCnt - 1) ? (arraySize - 1) : (indices[i][1] + chunk_size - 1); // End index
    }
}

// Get a random number in the range [x, y]
int GetRand(int x, int y)
{
    int r = rand();
    r = x + r % (y - x + 1);
    return r;
}

// Function to get the current time in milliseconds
long GetCurrentTime(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0)
    {
        perror("gettimeofday failed");
        exit(EXIT_FAILURE);
    }
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

void SetTime(void)
{
    gRefTime = GetCurrentTime();
}

long GetTime(void)
{
    return GetCurrentTime() - gRefTime;
}

int ComputeTotalProduct()
{
    int prod = 1;
    for (int i = 0; i < gThreadCount; i++)
    {
        prod = (prod * gThreadProd[i]) % NUM_LIMIT;
    }
    return prod;
}