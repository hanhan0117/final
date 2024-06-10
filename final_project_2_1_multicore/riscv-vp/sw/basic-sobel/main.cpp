/*
#include <cstdio>
#include <cstdlib>

#include "cassert"
#include "math.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string"
#include "string.h"
using namespace std;
#include <time.h>

#include <iostream>
*/
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include "math.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
//
int sem_init(uint32_t *__sem, uint32_t count) __THROW {
	*__sem = count;
	return 0;
}

int sem_wait(uint32_t *__sem) __THROW {
	uint32_t value, success;  // RV32A
	__asm__ __volatile__(
	    "\
L%=:\n\t\
     lr.w %[value],(%[__sem])            # load reserved\n\t\
     beqz %[value],L%=                   # if zero, try again\n\t\
     addi %[value],%[value],-1           # value --\n\t\
     sc.w %[success],%[value],(%[__sem]) # store conditionally\n\t\
     bnez %[success], L%=                # if the store failed, try again\n\t\
"
	    : [value] "=r"(value), [success] "=r"(success)
	    : [__sem] "r"(__sem)
	    : "memory");
	return 0;
}

int sem_post(uint32_t *__sem) __THROW {
	uint32_t value, success;  // RV32A
	__asm__ __volatile__(
	    "\
L%=:\n\t\
     lr.w %[value],(%[__sem])            # load reserved\n\t\
     addi %[value],%[value], 1           # value ++\n\t\
     sc.w %[success],%[value],(%[__sem]) # store conditionally\n\t\
     bnez %[success], L%=                # if the store failed, try again\n\t\
"
	    : [value] "=r"(value), [success] "=r"(success)
	    : [__sem] "r"(__sem)
	    : "memory");
	return 0;
}

int barrier(uint32_t *__sem, uint32_t *__lock, uint32_t *counter, uint32_t thread_count) {
	sem_wait(__lock);
	if (*counter == thread_count - 1) {  // all finished
		*counter = 0;
		sem_post(__lock);
		for (int j = 0; j < thread_count - 1; ++j) sem_post(__sem);
	} else {
		(*counter)++;
		sem_post(__lock);
		sem_wait(__sem);
	}
	return 0;
}

// Total number of cores
// static const int PROCESSORS = 2;
#define PROCESSORS 2
// the barrier synchronization objects
uint32_t barrier_counter = 0;
uint32_t barrier_lock;
uint32_t barrier_sem;
// the mutex object to control global summation
uint32_t lock;
// print synchronication semaphore (print in core order)
uint32_t print_sem[PROCESSORS];
// global summation variable
float pi_over_4 = 0;
//
union word {
	int sint;
	unsigned int uint;
	unsigned char uc[4];
};

unsigned int input_rgb_raw_data_offset;
const unsigned int output_rgb_raw_data_offset = 54;
int width;
int height;
unsigned int width_bytes;
unsigned char bits_per_pixel;
unsigned short bytes_per_pixel;
unsigned char *source_bitmap;
unsigned char *target_bitmap;
const int WHITE = 255;
const int BLACK = 0;
const int THRESHOLD = 90;

// Sobel Filter ACC
static char *const SOBELFILTER_START_ADDR0 = reinterpret_cast<char *const>(0x73000000);
static char *const SOBELFILTER_READ_ADDR0 = reinterpret_cast<char *const>(0x73000004);
static char *const SOBELFILTER_START_ADDR1 = reinterpret_cast<char *const>(0x73000000);
static char *const SOBELFILTER_READ_ADDR1 = reinterpret_cast<char *const>(0x73000004);

// DMA
static volatile uint32_t *const DMA_SRC_ADDR0 = (uint32_t *const)0x70000000;
static volatile uint32_t *const DMA_DST_ADDR0 = (uint32_t *const)0x70000004;
static volatile uint32_t *const DMA_LEN_ADDR0 = (uint32_t *const)0x70000008;
static volatile uint32_t *const DMA_OP_ADDR0 = (uint32_t *const)0x7000000C;
static volatile uint32_t *const DMA_STAT_ADDR0 = (uint32_t *const)0x70000010;
static const uint32_t DMA_OP_MEMCPY = 1;

static volatile uint32_t *const DMA_SRC_ADDR1 = (uint32_t *const)0x70001000;
static volatile uint32_t *const DMA_DST_ADDR1 = (uint32_t *const)0x70001004;
static volatile uint32_t *const DMA_LEN_ADDR1 = (uint32_t *const)0x70001008;
static volatile uint32_t *const DMA_OP_ADDR1 = (uint32_t *const)0x7000100C;
static volatile uint32_t *const DMA_STAT_ADDR1 = (uint32_t *const)0x70001010;


bool _is_using_dma = true;

void write_data_to_ACC0(char *ADDR, volatile unsigned char *buffer, int len) {
	if (_is_using_dma) {
		// Using DMA
		*DMA_SRC_ADDR0 = (uint32_t)(buffer);
		*DMA_DST_ADDR0 = (uint32_t)(ADDR);
		*DMA_LEN_ADDR0 = len;
		*DMA_OP_ADDR0 = DMA_OP_MEMCPY;
	} else {
		// Directly Send
		memcpy(ADDR, (void *)buffer, sizeof(unsigned char) * len);
	}
}
void write_data_to_ACC1(char *ADDR, volatile unsigned char *buffer, int len) {
	if (_is_using_dma) {
		// Using DMA
		*DMA_SRC_ADDR1 = (uint32_t)(buffer);
		*DMA_DST_ADDR1 = (uint32_t)(ADDR);
		*DMA_LEN_ADDR1 = len;
		*DMA_OP_ADDR1 = DMA_OP_MEMCPY;
	} else {
		// Directly Send
		memcpy(ADDR, (void *)buffer, sizeof(unsigned char) * len);
	}
}
void read_data_from_ACC0(char *ADDR, volatile unsigned char *buffer, int len) {
	if (_is_using_dma) {
		// Using DMA
		*DMA_SRC_ADDR0 = (uint32_t)(ADDR);
		*DMA_DST_ADDR0 = (uint32_t)(buffer);
		*DMA_LEN_ADDR0 = len;
		*DMA_OP_ADDR0 = DMA_OP_MEMCPY;
	} else {
		// Directly Read
		memcpy((void *)buffer, ADDR, sizeof(unsigned char) * len);
	}
}
void read_data_from_ACC1(char *ADDR, volatile unsigned char *buffer, int len) {
	if (_is_using_dma) {
		// Using DMA
		*DMA_SRC_ADDR1 = (uint32_t)(ADDR);
		*DMA_DST_ADDR1 = (uint32_t)(buffer);
		*DMA_LEN_ADDR1 = len;
		*DMA_OP_ADDR1 = DMA_OP_MEMCPY;
	} else {
		// Directly Read
		memcpy((void *)buffer, ADDR, sizeof(unsigned char) * len);
	}
}

int main(unsigned hart_id) {
	/////////////////////////////
	// thread and barrier init //
	/////////////////////////////

	if (hart_id == 0) {
		// create a barrier object with a count of PROCESSORS
		sem_init(&barrier_lock, 1);
		sem_init(&barrier_sem, 0);  // lock all cores initially
		for (int i = 0; i < PROCESSORS; ++i) {
			sem_init(&print_sem[i], 0);  // lock printing initially
		}
		// Create mutex lock
		sem_init(&lock, 1);
	}
	//
	volatile unsigned char buffer[4] = {0};
	word data;
	int done;
	int result1[128];
	int result2[128];
	printf("Start processing...");
	printf("\n");
	int input[256];

	for (int i = 0; i < 128; i++) {
		input[i] = (i * 119) % 128;
		//printf("%d\n", input[i]);
	}
	printf("End of Pattern Generation\n");
	// sc_time start_time = sc_time_stamp();

	sem_wait(&lock);
	if (hart_id = 0) {
		printf("input of core%d\n", hart_id);
		printf("core0\n");
		for (int i = 0; i < 128; i = i + 4) {
			buffer[0] = input[i];
			buffer[1] = input[i + 1];
			buffer[2] = input[i + 2];
			buffer[3] = input[i + 3];
			printf("%d\n", input[i]);
			printf("%d\n", input[i + 1]);
			printf("%d\n", input[i + 2]);
			printf("%d\n", input[i + 3]);
			//sem_wait(&lock);
			write_data_to_ACC0(SOBELFILTER_START_ADDR0, buffer, 4);
			//sem_post(&lock);
		}
	}
	sem_post(&lock);

	sem_wait(&lock);
	sem_post(&lock);
	sem_wait(&lock);
	sem_post(&lock);

	sem_wait(&lock);
	if (hart_id = 1) {
		printf("input of core%d\n", hart_id);
		printf("core1\n");
		for (int i = 0; i < 128; i = i + 4) {
			buffer[0] = input[i];
			buffer[1] = input[i + 1];
			buffer[2] = input[i + 2];
			buffer[3] = input[i + 3];
			printf("%d\n", input[i]);
			printf("%d\n", input[i + 1]);
			printf("%d\n", input[i + 2]);
			printf("%d\n", input[i + 3]);
			
			write_data_to_ACC1(SOBELFILTER_START_ADDR1, buffer, 4);
			
		}
	}
	sem_post(&lock);

	sem_wait(&lock);
	int valid = 0;
	int i = 0;
	done = 0;
	sem_post(&lock);

	sem_wait(&lock);
	if(hart_id == 0){
		while (!done) {
			buffer[0] = 0;
			buffer[1] = 0;
			buffer[2] = 0;
			buffer[3] = 0;
			
			read_data_from_ACC0(SOBELFILTER_READ_ADDR0, buffer, 4);

			if (i < 126) {
				valid = 0;
				while (!valid) {
					result1[i] = buffer[0];
					result1[i + 1] = buffer[1];
					result1[i + 2] = buffer[2];
					valid = buffer[3];
				}
				//sem_post(&lock);
				i = i + 3;
			} else {
				done = 0;
				while (!done) {
					result1[i] = buffer[0];
					result1[i + 1] = buffer[1];
					done = buffer[3];
				}
				//sem_post(&lock);
				i = i + 3;
			}
		}
	}
	
	sem_post(&lock);

	sem_wait(&lock);
	valid = 0;
	i = 0;
	done = 0;
	sem_post(&lock);

	sem_wait(&lock);
	if(hart_id == 1){
		while (!done) {
			buffer[0] = 0;
			buffer[1] = 0;
			buffer[2] = 0;
			buffer[3] = 0;
			sem_wait(&lock);
			read_data_from_ACC1(SOBELFILTER_READ_ADDR1, buffer, 4);

			if (i < 126) {
				valid = 0;
				while (!valid) {
					result2[i] = buffer[0];
					result2[i + 1] = buffer[1];
					result2[i + 2] = buffer[2];
					valid = buffer[3];
				}
				sem_post(&lock);
				i = i + 3;
			} else {
				done = 0;
				while (!done) {
					result2[i] = buffer[0];
					result2[i + 1] = buffer[1];
					done = buffer[3];
				}
				sem_post(&lock);
				i = i + 3;
			}
		}
	}
	
	sem_post(&lock);

	////////////////////////////
	// barrier to synchronize //
	////////////////////////////
	// Wait for all threads to finish
	barrier(&barrier_sem, &barrier_lock, &barrier_counter, PROCESSORS);
	//

	printf("Result from core0:\n");
	for (int i = 0; i < 128; i++) {
		printf("%d\n", result1[i]);
	}
	printf("Result from core1:\n");
	for (int i = 0; i < 128; i++) {
		printf("%d\n", result2[i]);
	}
}
