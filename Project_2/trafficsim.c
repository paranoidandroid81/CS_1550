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

int main()
{
  srand(time(NULL));                //one-time random generation
  //# of used buffer slots
  struct cs1550_sem* full;
  //# of empty buffer slots
  struct cs1550_sem* empty;
  //controls access to critical
  struct cs1550_sem* mutex;
  //ask for semaphore memory (3 of them) in RAM
  struct cs1550_sem* sem_memory = (struct cs1550_sem*)mmap(NULL, sizeof(struct cs1550_sem)*3,
  PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  full = sem_memory;                  //first slot in RAM
  empty = sem_memory + 1;             //second slot in RAM
  mutex = sem_memory + 2;             //third slot in RAM
  full->value = 0;                    //initalized with 0 resources used
  empty->value = BUFFER_SIZE;         //all slots open for cars initially
  mutex->value = 1;                   //initally unlocked mutex

  //create shared memory for all possible processes in buffer from N direction, plus 2 for 'in' and 'out' in bounded buffer implementation
  unsigned int* north_buf = (unsigned int*)mmap(NULL, (BUFFER_SIZE + 2) * sizeof(unsigned int), PROT_READ|PROT_WRITE,
  MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  //create shared memory for all possible processes in buffer from S direction, plus 2 for 'in' and 'out' in bounded buffer implementation
  unsigned int* south_buf = (unsigned int*)mmap(NULL, (BUFFER_SIZE + 2) * sizeof(unsigned int), PROT_READ|PROT_WRITE,
  MAP_SHARED|MAP_ANONYMOUS, 0, 0);

}
