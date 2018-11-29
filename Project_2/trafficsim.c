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

#define BUFFER_SIZE 10            //max buffer always at 10 cars

//redefine semaphore struct for main file
struct cs1550_sem
{
	int value;
	struct proc_node* head;					//start of process queue
	struct proc_node* tail;					//end of process queue
};

//wrapper functions for semaphore system calls
void down(struct cs1550_sem *sem)
{
  syscall(__NR_cs1550_down, sem);
}

void up(struct cs1550_sem *sem)
{
  syscall(__NR_cs1550_up, sem);
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

  //create shared memory for all possible processes in buffer from N direction, plus 2 for 'in' and 'out' in bounded buffer implementation, +1 for horn
  unsigned int* north_buf = (unsigned int*)mmap(NULL, (BUFFER_SIZE + 4) * sizeof(unsigned int), PROT_READ|PROT_WRITE,
  MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  //create shared memory for all possible processes in buffer from S direction, plus 2 for 'in' and 'out' in bounded buffer implementation, + 1 for sleeping
  unsigned int* south_buf = (unsigned int*)mmap(NULL, (BUFFER_SIZE + 4) * sizeof(unsigned int), PROT_READ|PROT_WRITE,
  MAP_SHARED|MAP_ANONYMOUS, 0, 0);

	//'in' ptr for north traffic
	unsigned int* prod_north = north_buf + BUFFER_SIZE;
	//'in' ptr for south traffic
	unsigned int* prod_south = south_buf + BUFFER_SIZE;
	//initalize both to 0
	*prod_north = 0;
	*prod_south = 0;

	//'out' ptr for north traffic
	unsigned int* cons_north = north_buf + (BUFFER_SIZE + 1);
	//'out' ptr for south traffic
	unsigned int* cons_south = south_buf + (BUFFER_SIZE + 1);
	//initialize both to 0
	*cons_north = 0;
	*cons_south = 0;
	//flag for sleeping flagperson
	unsigned int* sleeping = south_buf + (BUFFER_SIZE + 2);
	*sleeping = 0;						//initally not sleeping
	//flag for if horn already blown
	unsigned int* blown = north_buf + (BUFFER_SIZE + 2);
	*blown = 0;

	//number of car throughout program
	unsigned int* car_num = south_buf + (BUFFER_SIZE + 3);
	*car_num = 0;

	//number of time at beginning, used to calculate time elapsed
	unsigned int* init_time = north_buf + (BUFFER_SIZE + 3);
	*init_time = (int)time(NULL);

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
			car = *car_num;				//get car number
			north_buf[*prod_north % BUFFER_SIZE] = car;									//insert car into north buf
			//print out car arriving event
			printf("Car %d coming from the N direction arrived in the queue at time %d.\n", car, ((int)time(NULL) - (*init_time)));
			//check if flagperson was sleeping, wake up if so
			if (*sleeping && !(*blown))
			{
				printf("Car %d coming from the N direction, blew their horn at time %d.\n", car, ((int)time(NULL) - (*init_time)));
				*blown = 1;
			}
			*prod_north = (*prod_north + 1) % BUFFER_SIZE;
			*car_num = *car_num + 1;
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
			car = *car_num;				//get car number
			south_buf[*prod_south % BUFFER_SIZE] = car;									//insert car into south buf
			//print out car arriving event
			printf("Car %d coming from the S direction arrived in the queue at time %d.\n", car, ((int)time(NULL) - (*init_time)));
			//check if flagperson was sleeping, wake up if so
			if (*sleeping && !(*blown))
			{
				printf("Car %d coming from the S direction, blew their horn at time %d.\n", car, ((int)time(NULL) - (*init_time)));
				*blown = 1;
			}
			*prod_south = (*prod_south + 1) % BUFFER_SIZE;
			*car_num = *car_num + 1;
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
		int car;
		while (1)							//consume indefinitely
		{
			//no cars at either end, flagperson going to sleep, needs to wait for wakeup
			if (full_north->value == 0 && full_south->value == 0)
			{
				if (!(*sleeping))
				{
					printf("The flagperson is now asleep.\n");
					*sleeping = 1;
				}
				continue;							//iterate again until new cars found
			}
			//cars to be taken from buffer
			if (*sleeping)					//was sleeping, now awake
			{
				printf("The flagperson is now awake.\n");
				*sleeping = 0;
				*blown = 0;
			}

			//currently consuming in N and S not full (10), consume in N until empty or until S full

			while (full_north->value > 0)
			{
				down(full_north);						//removing a resource
				down(mutex);						//entering critical section
				car = north_buf[(*cons_north % BUFFER_SIZE)];					//consume car from buffer
				sleep(2);							//each car consumed takes 2s to go thru
				printf("Car %d coming from the N direction left the construction zone at time %d.\n",
				car, ((int)time(NULL) - (*init_time)));
				*cons_north = (*cons_north + 1) % BUFFER_SIZE;						//increment 'out'
				up(mutex);							//exiting critical section
				up(empty_north);							//new empty spot from consumption
				if (full_south->value == 10)
				{
					break;									//need to switch to other side, queue full
				}
			}
			//currently consuming in S and N not full (10), consume in S until empty or until S full
			while (full_south->value > 0)
			{
				down(full_south);						//removing a resource
				down(mutex);						//entering critical section
				car = south_buf[(*cons_south % BUFFER_SIZE)];					//consume car from buffer
				sleep(2);							//each car consumed takes 2s to go thru
				printf("Car %d coming from the S direction left the construction zone at time %d.\n",
				car, ((int)time(NULL) - (*init_time)));
				*cons_south = (*cons_south + 1) % BUFFER_SIZE;						//increment 'out'
				up(mutex);							//exiting critical section
				up(empty_south);							//new empty spot from consumption
				if (full_north->value == 10)
				{
					break;									//need to switch to other side, queue full
				}
			}
		}
	}
	//have parent (main) process wait, looping, until all children are complete, avoids zombies or orphans
	while(wait(NULL) > 0);
	return 0;
}
