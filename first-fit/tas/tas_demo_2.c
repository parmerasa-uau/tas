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

typedef struct process_args {
	int foo;
} process_args_t;

void * process(void * args) {
	int n, count = 0;

	struct process_args * my_data;
	my_data = (struct process_args *) args;

	n = my_data->foo;

	while(n > 1) {
		if(n % 2 == 0) n = n/2;
		else n = 3 * n + 1;
		count++;
	}

	PFL
	printf("WORKER THREAD Collatz of %i => %i iterations\n", my_data->foo, count);
	PFU

	return NULL;
}

// Data structures for data parallel pattern
#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile process_args_t dp_a_args_data[128];
SHARED_VARIABLE(membase_uncached0) volatile void * dp_a_args[128];
SHARED_VARIABLE(membase_uncached0) volatile tas_dataparallel_t dp_a = {
		(tas_runnable_t) process, dp_a_args, 128 };
#else
process_args_t dp_a_args_data[128];
void * dp_a_args[128];
tas_dataparallel_t dp_a = {(tas_runnable_t) process, dp_a_args, 128};
#endif

void * process_aa(void * args) {
	int i;

	struct process_args * my_data;
	my_data = (struct process_args *) args;

	PFL
	printf("WORKER THREAD AA Working on %i \n", my_data->foo);
	PFU

	for (i = 0; i < 128; i++)
		dp_a_args_data[i].foo = i + 100;
	for (i = 0; i < 128; i++)
		dp_a_args[i] = &(dp_a_args_data[i]);

	tas_dataparallel_init(&dp_a, 6);
	tas_dataparallel_execute(&dp_a);
	tas_dataparallel_finalize(&dp_a);

	return NULL;
}

// Data structures for data parallel pattern
#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile process_args_t dp_b_args_data[64];
SHARED_VARIABLE(membase_uncached0) volatile void * dp_b_args[64];
SHARED_VARIABLE(membase_uncached0) volatile tas_dataparallel_t dp_b = {
		(tas_runnable_t) process, dp_b_args, 64 };
#else
process_args_t dp_b_args_data[64];
void * dp_b_args[64];
tas_dataparallel_t dp_b = {(tas_runnable_t) process, dp_b_args, 64};
#endif


void * process_ab(void * args) {
	int i;

	struct process_args * my_data;
	my_data = (struct process_args *) args;

	PFL
	printf("WORKER THREAD AB Working on %i \n", my_data->foo);
	PFU

	for (i = 0; i < 64; i++)
		dp_b_args_data[i].foo = i + 200;
	for (i = 0; i < 64; i++)
		dp_b_args[i] = &(dp_b_args_data[i]);

	tas_dataparallel_init(&dp_b, 4);
	tas_dataparallel_execute(&dp_b);
	tas_dataparallel_finalize(&dp_b);

	return NULL;
}

void * process_ac(void * args) {
	struct process_args * my_data;
	my_data = (struct process_args *) args;

	PFL
	printf("WORKER THREAD AC Working on %i \n", my_data->foo);
	PFU

	return NULL;
}

void * process_ad(void * args) {
	struct process_args * my_data;
	my_data = (struct process_args *) args;

	PFL
	printf("WORKER THREAD AD Working on %i \n", my_data->foo);
	PFU

	return NULL;
}

// Data structures for task parallel pattern
#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile process_args_t task_parallelism_arguments[4];
SHARED_VARIABLE(membase_uncached0) volatile void * task_parallelism_args[4];
SHARED_VARIABLE(membase_uncached0) volatile tas_runnable_t task_parallelism_runnables[] = {
		 (tas_runnable_t) process_aa,
				(tas_runnable_t) process_ab, (tas_runnable_t) process_ac,
				(tas_runnable_t) process_ad};
SHARED_VARIABLE(membase_uncached0) volatile tas_taskparallel_t task_parallelism = {
		task_parallelism_runnables, task_parallelism_args, 4};
#else
process_args_t task_parallelism_arguments[4];
void * task_parallelism_args[4];
tas_runnable_t task_parallelism_runnables[] = { (tas_runnable_t) process_aa,
		(tas_runnable_t) process_ab, (tas_runnable_t) process_ac,
		(tas_runnable_t) process_ad };
tas_taskparallel_t task_parallelism = { task_parallelism_runnables, task_parallelism_args, 4 };
#endif

void demo_2_main_core_0() {
	int i;

	// Task Parallel Pattern
	for (i = 0; i < 4; i++)
		task_parallelism_arguments[i].foo = i;
	for (i = 0; i < 4; i++)
		task_parallelism_args[i] = &(task_parallelism_arguments[i]);

	tas_taskparallel_init(& task_parallelism, 2);
	tas_taskparallel_execute(&task_parallelism);
	tas_taskparallel_finalize(&task_parallelism);

	PFL
	printf("TEST\n");
	PFU
}
