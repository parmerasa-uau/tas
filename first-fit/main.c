/*
 * main.c
 *
 *      Author: ralf
 */

#include "tas/tas.h"
#include "tas/tas_stdio.h"
#include "platform.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#define SHARED_VARIABLE(foo) /* shared_variable */

// #include "tas/tas_demo_0.h"
// #define demo_0_main_core_0(foo) main_core_0(foo)

// Matrix Multiplication (Data Parallel)
#include "tas/tas_demo_3.h"
#define MAIN_CORE_0() demo_3_main_core_0()

// Nested Patterns
// #include "tas/tas_demo_2.h"
// #define MAIN_CORE_0() demo_2_main_core_0()

// Genetic Algorithm (Data Parallel)
// #include "tas/tas_demo_5.h"
// #define MAIN_CORE_0() demo_5_main_core_0()

// CCR (Data Parallel)
// #include "tas/tas_demo_6.h"
// #define MAIN_CORE_0() demo_6_main_core_0()

// FFT (Pipeline)
// #include "tas/tas_demo_fft.h"
// #define MAIN_CORE_0() demo_fft_init()


pthread_barrier_t sync_cpu_start;
pthread_barrier_t sync_cpu_end;
pthread_mutex_t lock_main_printf = PTHREAD_MUTEX_INITIALIZER;


int main(void) {
	const unsigned int cpu = 0, core = 0, cluster = 0;

	tas_init(WORKERS);

	MAIN_CORE_0();

	tas_finalize();
	
	exit(0);

	return 0;
}
