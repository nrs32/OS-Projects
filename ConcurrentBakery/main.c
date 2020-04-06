#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h> // For srand
#include <time.h>   // For time
#include <string.h> // For strcmp
#include <stdint.h> // For intptr_t
#include <fcntl.h>  // For O_CREAT
#include <unistd.h> // For usleep

// Bread we have
#define NUM_BREADS 6
char* breads[NUM_BREADS] = { "Rye", "Sourdough", "Pumpkin", "French", "Zucchini", "Wheat"};

// Global semaphores
#define REMAINING_ROOM_FOR_CUSTOMERS "/REMAINING_ROOM_FOR_CUSTOMERS"
#define BUSY_MUTEX "/BUSY_MUTEX"
sem_t *remaining_room_for_customers, *busy_mutex;
int num_loaves_on_shelf = 0; // protected by busy_mutex


///////////////////////////////
/* This is a customer thread */
void *customer(void *arg)
{
	// Wait for room in the bakery so I can enter
	sem_wait(remaining_room_for_customers);
    fprintf(stderr, "Customer %ld: \"I'm in the bakery!\"\n", (intptr_t) arg);
	
	usleep(2000000);

	// Once customer exits the bakery
	// Notify those waiting outside that there is room for someone to enter
	sem_post(remaining_room_for_customers);
    fprintf(stderr, "Customer %ld: \"I'm leaving!\"\n", (intptr_t) arg);

	return NULL;
}
///////////////////////////////////////////
/* This thread is responsible for baking */
void *baker(void *arg)
{
	srand(time(0)); // Seed rand number generator

	// We only bake 10 loaves of bread a day
	for (int i = 0; i < 10; i++) {
		// Obtain mutex for busy (we are not with a customer)
		sem_wait(busy_mutex);

		// Bake the bread
		fprintf(stderr, "Baker: \"I'm baking %s bread.\"\n", breads[rand() % (NUM_BREADS - 1)]);
		usleep(2000000);

		// Add to loaf to bread shelf (protected by busy semiphore)
		num_loaves_on_shelf++;

		// Finished baking
		if (i == 9) { 
			fprintf(stderr, "Baker: \"Finished baking for today!\"\n"); 
		}

		// Release semaphore for busy (we are not baking)
		sem_post(busy_mutex);
	}

	return NULL;
}

///////////////////////////////////////////////
/* This thread is responsible for cashiering */
void *cashier(void *arg)
{
	// Since only bake 10 loaves of bread a day
	// We can only serve 10 customers
	for (int i = 0; i < 10; i++) {

		// Spin lock. Before we can ask to be busy cashiering
		// we must have loaves to give customer
		while (num_loaves_on_shelf == 0);

		// Obtain semaphore for busy (we are not baking)
		sem_wait(busy_mutex);

		// Remove loaf from bread shelf (protected by busy semiphore)
		num_loaves_on_shelf--;
		fprintf(stderr, "Baker: \"Here is your loaf!\"\n");
		usleep(1000000);

		// Release semaphore for busy (we are not with a customer)
		sem_post(busy_mutex);
	}

	return NULL;
}

//////////////////////////////////
/* Create semapores and threads. /
 * Open and close bakery.       */
int main(int argc, char *argv[])
{
	// TODO: remove debug if not needed
	int isDebug = argc == 2 && !strcmp(argv[1], "!") ? 1 : 0;
    fprintf(stderr, "Time for the Bakery to open!\n"); 
	
	// Counting Semaphore ensuring a max of 5 customers in bakery at a time
	remaining_room_for_customers = sem_open(REMAINING_ROOM_FOR_CUSTOMERS, O_CREAT, NULL, 5);
	if (remaining_room_for_customers == NULL) {
		perror("Creating remaining_room_for_customers semaphore failed"); 
	}

	// Mutex Semaphore to determine if baker is busy
	busy_mutex = sem_open(BUSY_MUTEX, O_CREAT, NULL, 1);
	if (busy_mutex == NULL) {
		perror("Creating busy_mutex semaphore failed"); 
	}

	// Create baker
	// We always start baking first
	pthread_t bakerThread;
    pthread_create(&bakerThread, 0, baker, NULL);

	pthread_t cashierThread;
    pthread_create(&cashierThread, 0, cashier, NULL);

    // Create customers
    pthread_t ts[10];

   	for (int i = 0; i < 10; i++)
		pthread_create(&ts[i], NULL, customer, (void *) (intptr_t) i);
	
	// Wait for all threads to exit before continuing in main
	for (int i = 0; i < 10; i++)
		pthread_join(ts[i], NULL);

	pthread_join(cashierThread, NULL);
	pthread_join(bakerThread, NULL);

    fprintf(stderr, "Bakery closed.\n"); 

	// Unlink the semaphores we used
	sem_unlink(REMAINING_ROOM_FOR_CUSTOMERS);
	sem_unlink(BUSY_MUTEX);

    // Wait for last thread to terminate and then exit main
	// *Instead of pthread_exit() we use pthread_join so we can unlink semaphores
    pthread_exit(NULL);

    return 0;
}