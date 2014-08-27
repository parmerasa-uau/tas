#include "tas.h"
#include "tas_stdio.h"


#include "tas_demo_0.h"

#include "../platform.h"

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

typedef struct process_args {
	int foo;
} process_args_t;

void * process(void * args) {
	struct process_args * my_data;
	my_data = (struct process_args *) args;

	PFL
	printf("WORKER THREAD DP Working on %i \n", my_data->foo);
	PFU

	return NULL;
}

void * process_a(void * args) {
	struct process_args * my_data;
	my_data = (struct process_args *) args;

	PFL
	printf("WORKER THREAD A Working on %i \n", my_data->foo);
	PFU

	return NULL;
}

void * process_b(void * args) {
	struct process_args * my_data;
	my_data = (struct process_args *) args;

	PFL
	printf("WORKER THREAD B Working on %i \n", my_data->foo);
	PFU

	return NULL;
}

// Data structures for data parallel pattern
#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile process_args_t dp_main_args_data[42];
SHARED_VARIABLE(membase_uncached0) volatile void * dp_main_args[42];
SHARED_VARIABLE(membase_uncached0) volatile tas_dataparallel_t dp_main = {
		(tas_runnable_t) process, dp_main_args, 42 };
#else
process_args_t dp_main_args_data[42];
void * dp_main_args[42];
tas_dataparallel_t dp_main = {(tas_runnable_t) process, dp_main_args, 42};
#endif

// Data structures for task parallel pattern
#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile process_args_t tp_main_args_data[2];
SHARED_VARIABLE(membase_uncached0) volatile void * tp_main_args[2];
SHARED_VARIABLE(membase_uncached0) volatile tas_runnable_t tp_runnables[] = {
		(tas_runnable_t) process_a, (tas_runnable_t) process_b };
SHARED_VARIABLE(membase_uncached0) volatile tas_taskparallel_t tp_main = {
		tp_runnables, tp_main_args, 2 };
#else
process_args_t tp_main_args_data[2];
void * tp_main_args[2];
tas_runnable_t tp_runnables [] = {(tas_runnable_t) process_a, (tas_runnable_t) process_b};
tas_taskparallel_t tp_main = {tp_runnables, tp_main_args, 2};
#endif

void demo_0_main_core_0() {
	int i;

	// Data Parallel Pattern
	for (i = 0; i < 42; i++)
		dp_main_args_data[i].foo = i;
	for (i = 0; i < 42; i++)
		dp_main_args[i] = &(dp_main_args_data[i]);

	tas_dataparallel_init(&dp_main, TOTAL_PROC_NUM);
	PFL
	printf(
			"MAIN - Now executing the data parallel pattern with %i workers...\n",
			dp_main.base.nr_threads);
	PFU
	tas_dataparallel_execute(&dp_main);

	tas_dataparallel_execute(&dp_main);

	tas_dataparallel_execute(&dp_main);

	tas_dataparallel_finalize(&dp_main);

	// Task Parallel Pattern
	for (i = 0; i < 2; i++)
		tp_main_args_data[i].foo = i;
	for (i = 0; i < 2; i++)
		tp_main_args[i] = &(tp_main_args_data[i]);

	tas_taskparallel_init(&tp_main, tp_main.nr_runnables);

	PFL
	printf(
			"MAIN - Now executing the task parallel pattern with %i workers...\n",
			tp_main.base.nr_threads);
	PFU

	tas_taskparallel_execute(&tp_main);
	tas_taskparallel_execute(&tp_main);
	tas_taskparallel_execute(&tp_main);
	tas_taskparallel_execute(&tp_main);

	tas_taskparallel_finalize(&tp_main);
}
