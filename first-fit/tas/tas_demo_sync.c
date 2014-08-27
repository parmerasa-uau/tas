#include "tas.h"

#if TAS_POSIX == 0
#include "system.h"
#include "kernel_lib.h"
#include "stdio.h"
#include "stdlib.h"
#else
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#endif

#include "tas_stdio.h"
#include "tas_demo_1.h"
#include "../platform.h"
#include "tas_stopwatch.h"

#define ITERATIONS 64

#define MEM_TOTAL 256
#define MEM_LOCAL 256

int data [MEM_TOTAL];
int durations [ITERATIONS];
int with_lock [ITERATIONS];

#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile ticketlock_t data_lock;
#else
pthread_mutex_t data_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

void analysis() {
	long sum_with_lock = 0;
	long sum_without_lock = 0;
	long sum_with_lock_nr = 0;
	long sum_without_lock_nr = 0;
	
	int i;
	
	for(i = 0; i < ITERATIONS; i++) {
		if(with_lock[i]) {
			sum_with_lock += durations[i];
			sum_with_lock_nr++;
		} else {
			sum_without_lock += durations[i];
			sum_without_lock_nr++;
		}
	}
	
	// PFL
	printf("AVG with lock: %i\n", (sum_with_lock / sum_with_lock_nr));
	printf("AVG without lock: %i\n", (sum_without_lock / sum_without_lock_nr));
	printf(
		"AVG difference: %i\n", 
		(sum_with_lock / sum_with_lock_nr) - (sum_without_lock / sum_without_lock_nr) 
	);
	// PFU
}

void demo_sync_main_core_0() {
	int i;

	PFL
	printf("Sync Demo\n");
	PFU
	
#if TAS_POSIX == 0
	ticket_init(&worker_available_lock);
#endif
	
	// INIT
	for(i = 0; i < MEM_TOTAL; i++) {
		data[i] = ( i * (i - 1) ) - ( (i + 2) * (i % 7) );
	}
	
	PFL
	printf("Init Done\n");
	PFU
	
	for(i = 0; i < ITERATIONS; i++) {
		int local_memory [MEM_LOCAL];
		int j, sum = 0;
		long volatile start;
		long volatile stop;
		
		char use_locks = (i % 7 == 0) || (i % 3 == 0);
		int offset = (i % (MEM_TOTAL / MEM_LOCAL)) * MEM_LOCAL;
		
		start = getTimestampHP();
		
		// Lock
		if(use_locks) { 
			pthread_mutex_lock(&data_lock);
		}
		
		// Read
		for(j = 0; j < MEM_LOCAL; j++) {
			local_memory[j] = data[j + offset];
		}
		
		// Unlock
		if(use_locks) { 
			pthread_mutex_unlock(&data_lock);
		}
		
		stop = getTimestampHP();
		
		durations[i] = (stop - start);
		with_lock[i] = use_locks;
		
		// Calculate
		for(j = 0; j < MEM_LOCAL; j++) {
			sum += local_memory[j];
		}
		
		// printf("Result %i in %i us\n", sum, (stop - start));
	}
	
	// printf("Locks: %i %i\n", locks_nr, (locks_sum / locks_nr));
	// printf("No_locks: %i %i\n", nolocks_nr, (nolocks_sum / nolocks_nr));
	// printf("Lock and unlock: %i\n", (locks_sum / locks_nr) - (nolocks_sum / nolocks_nr));
	
	analysis();
	
	/* for(i = 0; i < ITERATIONS; i++) {
		printf("%i;%i\n", with_lock[i], durations[i]);
	} */
	
	return;
}
