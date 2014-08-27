/*
 * main.c
 *
 *      Author: ralf
 */

#include "tas/tas.h"
#include "tas/tas_stdio.h"
#include "platform.h"

#if TAS_POSIX == 0
#include "system.h"
#include "kernel_lib.h"
#include "stdio.h"
#include "stdlib.h"
#else
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#define SHARED_VARIABLE(foo) /* shared_variable */
#endif

// #include "tas/tas_demo_0.h"
// #define demo_0_main_core_0(foo) main_core_0(foo)

// Matrix Multiplication (Data Parallel)
// #include "tas/tas_demo_3.h"
// #define MAIN_CORE_0() demo_3_main_core_0()

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
#include "tas/tas_demo_fft.h"
#define MAIN_CORE_0() demo_fft_init()

/* int corenum() {
    return 99;
}

int clusternum() {
    return 77;
} */


#if TAS_POSIX == 0
/* membase_shared0 has to be declared in ldscript; see also segmentation.h */SHARED_VARIABLE(membase_uncached0) volatile unsigned int max_interrupts =
		16;
SHARED_VARIABLE(membase_uncached0) volatile barrier_t sync_cpu_start = {.waiting = 0, .count = TOTAL_PROC_NUM, };
SHARED_VARIABLE(membase_uncached0) volatile barrier_t sync_cpu_end = {.waiting = 0, .count = TOTAL_PROC_NUM, };
SHARED_VARIABLE(membase_uncached0) volatile ticketlock_t lock_main_printf;
// SHARED_VARIABLE(membase_uncached0) volatile ticketlock_t lock_irq_printf;
#else
pthread_barrier_t sync_cpu_start;
pthread_barrier_t sync_cpu_end;
pthread_mutex_t lock_main_printf = PTHREAD_MUTEX_INITIALIZER;
#endif

#if TAS_POSIX == 0

#define TIMER_PERIOD_IN_CLOCK_CYCLES	100000

void _irq_timer_handler(void) {
	/* Set new timer value at the beginning to reduce jitter */
	set_timer(TIMER_PERIOD_IN_CLOCK_CYCLES);

	unsigned int tb = get_time_base();

	const unsigned int cpu = procnum();
	const unsigned int core = corenum();
	const unsigned int cluster = clusternum();

	unsigned int irq_to_go = fetch_and_add(&max_interrupts, -1);

	// ticket_lock(&lock_irq_printf);
	PFL
	printf(
			"Timer IRQ received at cycle %u on processor: %u cluster: %u core: %u. \n"
					"%u interrupts to go!\n\n", tb, cpu, cluster, core,
			--irq_to_go);
	// ticket_unlock(&lock_irq_printf);
	PFU

	if (!irq_to_go) {
		exit(0);
	}
}

void _exc_handler(unsigned int offset) {
	printf("Exception with vector offset %X\n", offset);
}
#endif



int main(void) {

#if TAS_POSIX == 0
	const unsigned int cpu = procnum();
	const unsigned int core = corenum();
	const unsigned int cluster = clusternum();
#else
	const unsigned int cpu = 0, core = 0, cluster = 0;
#endif

	/*
	 * CPU 0 has to initialize the barrier and lock variables and needs some
	 * extra execution time. So, without the barrier CPU 0 prints the
	 * "Hello ..." message last. With the barrier it prints the message first.
	 */
	if (cpu == 0) {

#if TAS_POSIX == 0
		// barrier_init(&sync_cpu_start);
		// barrier_init(&sync_cpu_end);
		ticket_init(&lock_main_printf);
#endif

		// 1 core for main, remainder for workers
		tas_init(WORKERS);

		PFL
		printf("STARTING INIT (%u cores) ON CORE: %u cluster: %u core: %u => MAIN\n", TOTAL_PROC_NUM, cpu,
				cluster, core);
		PFU

// Code from above should be here but cannot because it needs to be executed _really_ early.

		PFL
		printf("FINISHED INIT ON CORE: %u cluster: %u core: %u => MAIN\n", cpu,
				cluster, core);
		PFU
	}

// For parMERASA: Synchronisation of all cores at a barrier after init.
#if TAS_POSIX == 0
	barrier_wait(&sync_cpu_start);
#endif

	if (cpu == 0) {
		PFL
		printf("Hello from processor: %u cluster: %u core: %u => MAIN\n", cpu, cluster, core);
		PFU

		MAIN_CORE_0();

		tas_finalize();
	} else if(cpu < WORKERS + 1) {
		PFL
		printf("Hello from processor: %u cluster: %u core: %u => WORKER\n", cpu, cluster, core);
		PFU

		tas_thread((void *) &worker_threads_data[cpu - 1]);
	} else {
		PFL
		printf("Hello from processor: %u cluster: %u core: %u => SPARE\n", cpu, cluster, core);
		PFU

		PFL
		printf("===> This is core %i, I am not used. <===\n", cpu);
		PFU
	}

#if TAS_POSIX == 0
	barrier_wait(&sync_cpu_end);
#endif

	exit(0);

	return 0;
}
