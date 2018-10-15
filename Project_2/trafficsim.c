/*
 *  CS 1550 Project 2: Producer/Consumer Problem
 *  Implementation of traffic simulator using semaphores
 *  Author: Michael Korst (mpk44@pitt.edu)
 */

#include <linux/unistd.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>             //for rand generation
#include "unistd.h"               //for local run purposes

#define BUFFER_SIZE 10            //max buffer always at 10 cars

//redefine semaphore struct for main file
struct cs1550_sem
{
	int value;
	struct proc_node* head;					//start of process queue
	struct proc_node* tail;					//end of process queue
};

//wrapper functions for semaphore system calls
void down(cs1550_sem *sem)
{
  syscall(__NR_cs1550_down, sem);
}

void up(cs1550_sem *sem)
{
  syscall(__NR_cs1550_up, sem);
}

//helper method to consume a car
void consume(struct cs1550_sem** empty, struct cs1550_sem** full, struct cs1550_sem** lock,
unsigned int** buf, unsigned int** cons_ptr, char dir)
{
	int car;
	down(*full);						//removing a resource
	down(*lock);						//entering critical section
	car = *buf[**cons_ptr % BUFFER_SIZE];					//consume car from buffer
	sleep(2);							//each car consumed takes 2s to go thru
	printf("Car %d coming from the %c direction left the construction zone at time %d",
	car, dir, (int)time(NULL));
	**cons_ptr = (**cons_ptr + 1) % BUFFER_SIZE;						//increment 'out'
	up(*lock);							//exiting critical section
	up(*empty);							//new empty spot from consumption
}

int main()
{
  srand(time(NULL));                //one-time random generation
  //# of used buffer slots, one for north and south direction
  struct cs1550_sem* full_north;
	struct cs1550_sem* full_south;
  //# of empty buffer slots, one for north and south direction
  struct cs1550_sem* empty_north;
	struct cs1550_sem* empty_south;
  //controls access to critical
  struct cs1550_sem* mutex;
  //ask for semaphore memory (5 of them) in RAM
  struct cs1550_sem* sem_memory = (struct cs1550_sem*)mmap(NULL, sizeof(struct cs1550_sem)*5,
  PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  full_north = sem_memory;                  //first slot in RAM
  empty_north = sem_memory + 1;             //second slot in RAM
	full_south = sem_memory + 2;						//third slot in RAM
	empty_south = sem_memory + 3;						//fourth slot in RAM
  mutex = sem_memory + 4;             //fifth slot in RAM
  full_north->value = 0;                    //initalized with 0 resources used
  empty_north->value = BUFFER_SIZE;         //all slots open for cars initially
	full_south->value = 0;                    //initalized with 0 resources used
  empty_south->value = BUFFER_SIZE;         //all slots open for cars initially
  mutex->value = 1;                   //initally unlocked mutex

  //create shared memory for all possible processes in buffer from N direction, plus 2 for 'in' and 'out' in bounded buffer implementation
  unsigned int* north_buf = (unsigned int*)mmap(NULL, (BUFFER_SIZE + 2) * sizeof(unsigned int), PROT_READ|PROT_WRITE,
  MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  //create shared memory for all possible processes in buffer from S direction, plus 2 for 'in' and 'out' in bounded buffer implementation
  unsigned int* south_buf = (unsigned int*)mmap(NULL, (BUFFER_SIZE + 2) * sizeof(unsigned int), PROT_READ|PROT_WRITE,
  MAP_SHARED|MAP_ANONYMOUS, 0, 0);

	//'in' ptr for north traffic
	unsigned int* prod_north = north_buf;
	//'in' ptr for south traffic
	unsigned int* prod_south = south_buf;
	//initalize both to 0
	*prod_north = 0;
	*prod_south = 0;

	//'out' ptr for north traffic
	unsigned int* cons_north = north_buf + 1;
	//'out' ptr for south traffic
	unsigned int* cons_south = south_buf + 1;
	//initialize both to 0
	*cons_north = 0;
	*cons_south = 0;

	//begin traffic, create 2 producers (1 for N and 1 for S) and 1 consumer (flagperson)
	//produce and consumer according to traffic rules, switching directions as cars fill up queue
	//producer creation with child processes, North first then South
	if (fork() == 0)
	{
		int car;
		int r;						//random number for determining if another car appears
		while (1)						//produce indefinitely in North
		{
			down(empty_north);				//removing empty spot
			down(mutex);						//entering critical section
			car = *prod_north;				//get car number
			north_buf[*prod_north % BUFFER_SIZE] = car;									//insert car into north buf
			//print out car arriving event
			printf("Car %d coming from the N direction arrived in the queue at time %d\n", car, (int)time(NULL));
			*prod_north = (*prod_north + 1) % BUFFER_SIZE;
			up(mutex);								//out of critical section
			up(full_north);						//adding another resource
			r = rand() % 10;						//generate random to see if another car is coming
			if (r >= 8)
			{
				sleep(20);									//20% chance car doesn't arrive, must sleep before new car
			}
		}
	}
	//South producer
	if (fork() == 0)
	{
		int car;
		int r;						//random number for determining if another car appears
		while (1)						//produce indefinitely in South
		{
			down(empty_south);				//removing empty spot
			down(mutex);						//entering critical section
			car = *prod_south;				//get car number
			south_buf[*prod_south % BUFFER_SIZE] = car;									//insert car into south buf
			//print out car arriving event
			printf("Car %d coming from the S direction arrived in the queue at time %d\n", car, (int)time(NULL));
			*prod_south = (*prod_south + 1) % BUFFER_SIZE;
			up(mutex);								//out of critical section
			up(full_south);						//adding another resource
			r = rand() % 10;						//generate random to see if another car is coming
			if (r >= 8)
			{
				sleep(20);									//20% chance car doesn't arrive, must sleep before new car
			}
		}
	}

	//begin consumer code (flagperson)
	if (fork() == 0)
	{
		int sleeping = 0;				//flag to determine if flagperson was previously sleeping, now awake
		while (1)							//consume indefinitely
		{
			//no cars at either end, flagperson going to sleep, needs to wait for wakeup
			if (full_north->value == 0 && full_south->value == 0)
			{
				printf("The flagperson is now asleep.\n");
				sleeping = 1;
				continue;							//iterate again until new cars found
			}
			//cars to be taken from buffer
			if (sleeping)					//was sleeping, now awake
			{
				printf("The flagperson is now awake.\n");
				sleeping = 0;
			}
			//currently consuming in N and S not full (10), consume in N until empty or until S full
			while (full_north->value > 0 && full_south->value < 10)
			{
				consume(&empty_north, &full_north, &mutex, &north_buf, &cons_north, 'N');
			}
			//currently consuming in S and N not full (10), consume in S until empty or until S full
			while (full_south->value > 0 && full_north->value < 10)
			{
				consume(&empty_south, &full_south, &mutex, &south_buf, &cons_south, 'S');
			}
		}
	}
	//have parent (main) process wait, looping, until all children are complete, avoids zombies or orphans
	while(wait(NULL) > 0);
	return 0;
}
