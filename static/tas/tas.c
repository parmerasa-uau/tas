#include "tas.h"

#if TAS_POSIX == 1
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <inttypes.h>
#else
#include "stdio.h"
#include "stdlib.h"
#include "system.h"
#include "kernel_lib.h"
#endif

#include "tas_stdio.h"

#include "tas_log.h"

#if TAS_POSIX == 1
#define barrier_wait pthread_barrier_wait
#endif

// Handle which workers are available
#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile char worker_available[TAS_MAX_WORKERS];
SHARED_VARIABLE(membase_uncached0) volatile int worker_available_nr = 0;
SHARED_VARIABLE(membase_uncached0) volatile int worker_total_count = 0;
SHARED_VARIABLE(membase_uncached0) volatile ticketlock_t worker_available_lock;
SHARED_VARIABLE(membase_uncached0) volatile tas_thread_t worker_threads_data[TAS_MAX_WORKERS];
SHARED_VARIABLE(membase_uncached0) volatile barrier_t worker_init_barrier;
SHARED_VARIABLE(membase_uncached0) volatile char tas_abstract_init_first = 1;
#else
char worker_available[TAS_MAX_WORKERS];
int worker_available_nr = 0;
int worker_total_count = 0;
// char worker_available_initialized = 0;
pthread_mutex_t worker_available_lock = PTHREAD_MUTEX_INITIALIZER;
tas_thread_t worker_threads_data[TAS_MAX_WORKERS];
pthread_t thread_tasks[TAS_MAX_WORKERS];
pthread_barrier_t worker_init_barrier;
char tas_abstract_init_first = 1;
#endif



/**
 * Initialize the TAS thread pool -> create threads
 * @param workers Number of workers
 * */
void tas_init(int workers) {
	int i;

	logStart("tas_init");

	worker_total_count = workers;

#if TAS_POSIX == 0
	barrier_init(&worker_init_barrier, workers + 1);
	ticket_init(&worker_available_lock);
#else
	pthread_barrier_init(&worker_init_barrier, NULL, workers + 1);
#endif

#if TAS_DEBUG == 1
	PFL
	printf("MAIN - START tas_init with %i workers\n", workers);
	PFU
#endif

	// Initialize structures for threads
	for (i = 0; i < worker_total_count; i++) {
		worker_threads_data[i].do_shutdown = 0;
		worker_threads_data[i].work_assigned = 0;
		worker_threads_data[i].pattern_assigned = 0;
		worker_threads_data[i].worker_id = i;

		// Initialize barriers (n + 1)
#if TAS_POSIX == 1
		pthread_barrier_init(&(worker_threads_data[i].pattern_assigned_barrier_idle),
				NULL, 2);
		pthread_barrier_init(&(worker_threads_data[i].pattern_assigned_barrier),
				NULL, 2);
#else
		barrier_init(&(worker_threads_data[i].pattern_assigned_barrier), 2);
		barrier_init(&(worker_threads_data[i].pattern_assigned_barrier_idle), 2);
#endif
	}

	// Start worker threads
#if TAS_POSIX == 1
	for (i = 0; i < worker_total_count; i++) {
		pthread_create(&(thread_tasks[i]), NULL, tas_thread,
				&(worker_threads_data[i]));
	}
#else
	// Threads are started automatically
#endif

#if TAS_DEBUG == 1
	PFL
	printf("MAIN - END tas_init\n");
	PFU
#endif

	logEnd("tas_init");
}

/** Finalize the TAS thread pool -> destroy threads */
void tas_finalize() {
	int i;

	logStart("tas_finalize");

	if (tas_abstract_init_first) {
#if TAS_INFO == 1
		PFL
		printf("MAIN - Wait for workers... (worker_init_barrier) \n");
		PFU
#endif

		barrier_wait(&worker_init_barrier); // ID=tas_finalize_worker_init_barrier

#if TAS_INFO == 1
		PFL
		printf("MAIN - Passed worker_init_barrier.\n");
		PFU
#endif
		tas_abstract_init_first = 0;
	}

	// Set flags for shut down
	for (i = 0; i < worker_total_count; i++) {
		worker_threads_data[i].do_shutdown = 1;
		/* worker_threads_data[i].work_assigned = 0;
		 worker_threads_data[i].pattern_assigned = 0; */
	}


	// Wait for them to eat the poison and join threads
	for (i = 0; i < worker_total_count; i++) {
		barrier_wait(
				&(worker_threads_data[i].pattern_assigned_barrier_idle)); // ID=tas_finalize_pattern_assigned_barrier_idle
		barrier_wait(
				&(worker_threads_data[i].pattern_assigned_barrier)); // ID=tas_finalize_pattern_assigned_barrier
#if TAS_POSIX == 1
		pthread_join(thread_tasks[i], NULL);
#endif
	}


	logEnd("tas_finalize");
}

/** Get a defined idle worker thread */
int tas_worker_get(int id) {
	ticket_lock(&worker_available_lock); // ID=tas_worker_get_worker_available_lock
	if (worker_available_nr > 0) {
		if (id < TAS_MAX_WORKERS && worker_available[id]) {
			worker_available[id] = 0;
			worker_available_nr--;

#if TAS_DEBUG == 1
			PFL
			printf("tas_worker_get_def - Found worker %i\n", id);
			PFU
#endif
		} else {
			PFL
			printf("tas_worker_get_def - Worker %i not available! => EXIT\n", id);
			PFU

			exit(1);
		}

	} else {
		PFL
		printf("tas_worker_get_def - No worker available! => EXIT\n");
		PFU

		exit(1);
	}
	ticket_unlock(&worker_available_lock); // ID=tas_worker_get_worker_available_lock

	return id;
}

/** Return number of available workers */
int tas_get_workers_available() {
	int result;
	ticket_lock(&worker_available_lock); // ID=tas_get_workers_available_worker_available_lock
	result = worker_available_nr;
	ticket_unlock(&worker_available_lock); // ID=tas_get_workers_available_worker_available_lock
	return result;
}

/** Release a worker thread */
void tas_worker_release(int worker) {
	ticket_lock(&worker_available_lock); // ID=tas_worker_release_worker_available_lock

	worker_available[worker] = 1;
	worker_available_nr++;

#if TAS_DEBUG == 1
	PFL
	printf("tas_worker_release - Released worker %i\n", worker);
	PFU
#endif

	ticket_unlock(&worker_available_lock); // ID=tas_worker_release_worker_available_lock
}

/** Code to be executed by all the worker threads */
void * tas_thread(void * parameters_void) {
	tas_thread_t * param = (tas_thread_t *) parameters_void;

#if TAS_DEBUG
#if TAS_POSIX == 1
	PFL
	printf("T%i A - Starting...\n", param->worker_id);
	PFU
#else
	PFL
	printf("T%i A - Starting on core %i...\n", param->worker_id, procnum());
	PFU
#endif
#endif

	// Register this worker
	ticket_lock(&worker_available_lock); // ID=tas_thread_worker_available_lock
	worker_available_nr++;
	worker_available[param->worker_id] = 1;

#if TAS_INFO == 1
	PFL
	printf("T%i A - Now are %i workers available.\n", param->worker_id,
			worker_available_nr);
	PFU
#endif
	ticket_unlock(&worker_available_lock); // ID=tas_thread_worker_available_lock

#if TAS_DEBUG == 1
	PFL
	printf(
			"T%i A - Waiting for other workers and main to reach worker_init_barrier...\n",
			param->worker_id);
	PFU
#endif

	barrier_wait(&worker_init_barrier); // tas_thread_worker_init_barrier

#if TAS_DEBUG == 1
	PFL
	printf("T%i A - Passed worker_init_barrier.\n", param->worker_id);
	PFU
#endif

	// Wait for something to happen...

	while (1) { // ID=loop_runnable
#if TAS_DEBUG == 1
		PFL
		printf("T%i - NEXT ITERATION.\n", param->worker_id);
		PFU
#endif

		// Wait for pattern to be assigned
		if (!param->pattern_assigned) {
			// worker_threads_data[param->worker_id].parent = NULL;
#if TAS_INFO == 1
			PFL
			printf(
					"T%i A - waits for a pattern (pattern_assigned_barrier_idle and pattern_assigned_barrier).\n",
					param->worker_id);
			PFU
#endif

			barrier_wait(&param->pattern_assigned_barrier_idle); // ID=tas_thread_pattern_assigned_barrier_idle

			barrier_wait(&param->pattern_assigned_barrier); // ID=tas_thread_pattern_assigned_barrier

#if TAS_DEBUG == 1
			PFL
			printf("T%i A - passed pattern_assigned_barrier.\n",
					param->worker_id);
			PFU
#endif

			// Check if thread shall exit
			if (param->do_shutdown) {
#if TAS_INFO == 1
				PFL
				printf("T%i D - is shutting down.\n", param->worker_id);
				PFU
#endif
				break;
			} else {
				continue;
			}
		}

		// Pattern is assigned => wait for work
		if (param->pattern_assigned) {
#if TAS_INFO == 1
			PFL
			printf(
					"T%i B - is assigned to a pattern (%i threads) and waits for work (work_available_barrier).\n",
					param->worker_id,
					worker_threads_data[param->worker_id].parent->nr_threads);
			PFU
#endif

			// Wait for work to be available
			barrier_wait(
					&(worker_threads_data[param->worker_id].parent->work_available_barrier)); // ID=tas_thread_work_available_barrier

#if TAS_DEBUG == 1
			PFL
			printf("T%i B - passed work_available_barrier.\n",
					param->worker_id);
			PFU
#endif

			// Do work
			// if (!param->do_shutdown) {
			if (param->work_assigned) {
#if TAS_DEBUG == 1
				PFL
				printf("T%i C - got work.\n", param->worker_id);
				PFU
#endif
				tas_runnable_t foo = param->runnable;
				(*foo)(param->args);

				// Reset thread
				param->work_assigned = 0;
#if TAS_INFO == 1
				PFL
				printf("T%i C - finished working.\n", param->worker_id);
				PFU
#endif
			} else {
#if TAS_INFO == 1
				PFL
				printf("T%i C - did not get work assigned.\n",
						param->worker_id);
				PFU
#endif
			}

			// Wait for work done
#if TAS_DEBUG == 1
			PFL
			printf("T%i C - waits for work_done_barrier (%i threads)....\n",
					param->worker_id,
					worker_threads_data[param->worker_id].parent->nr_threads
							+ 1);
			PFU
#endif

			barrier_wait(&(worker_threads_data[param->worker_id].parent->work_done_barrier)); // ID=tas_thread_work_done_barrier

#if TAS_DEBUG == 1
			PFL
			printf("T%i C - passed work_done_barrier.\n", param->worker_id);
			PFU
#endif

			// }

			continue;
		}
	}

#if TAS_INFO == 1
	PFL
	printf("T%i D - is exiting. Good bye!\n", param->worker_id);
	PFU
#endif

	return NULL;
}

int get_next_worker(uint64_t workers, int last_worker) {
	int i;
	int top = sizeof(uint64_t) * 8;
	// if(last_worker == -1) last_worker = 0;

	// count = NumberOfSetBits(workers);

	for(i = last_worker + 1; i < top; i++) {
		uint64_t mask = 0x01;
		if( ( mask << i ) & workers) {
			// printf("  Found worker %i with mask %lu\n", i, mask);
			return i;
		}
	}

	return -1;
}

int count_workers(uint64_t workers) {
	int count = 0, i;
	int top = sizeof(uint64_t) * 8;

	// count = NumberOfSetBits(workers);

	for(i = 0; i < top; i++) {
		uint64_t mask = 0x01;
		if( ( mask << i ) & workers) {
			// printf("  Found worker %i with mask %lu\n", i, mask);
			count++;
		}
	}

	return count;
}

void tas_abstract_init(tas_abstract_t * base, uint64_t threads) {
	int i, nr_threads;

	nr_threads = count_workers(threads);

#ifdef TAS_STATISTICS
	logStartA("tas_abstract_init with %i threads", threads);
#endif

#if TAS_INFO == 1
	PFL
#if TAS_POSIX == 1
	printf("START tas_abstract_init with 0x%" PRIX64 " as workers, i.e., %i workers\n", threads, nr_threads);
#else
	printf("START tas_abstract_init with 0x%x as workers, i.e., %i workers\n", threads, nr_threads);
#endif
	PFU
#endif

	if (tas_abstract_init_first) {
#if TAS_INFO == 1
		PFL
		printf("MAIN - Wait for workers... (worker_init_barrier) \n");
		PFU
#endif

		barrier_wait(&worker_init_barrier); // ID=tas_abstract_init_worker_init_barrier

#if TAS_INFO == 1
		PFL
		printf("MAIN - Passed worker_init_barrier.\n");
		PFU
#endif

		tas_abstract_init_first = 0;
	}

	if (TAS_MAIN_IS_WORKER)
		base->nr_threads = nr_threads;
	else
		base->nr_threads = nr_threads;

	if (base->nr_threads > TAS_MAX_WORKERS)
		base->nr_threads = TAS_MAX_WORKERS;

	// Initialize barriers (n + 1) of pattern
#if TAS_POSIX == 1
	pthread_barrier_init(&base->work_available_barrier, NULL,
			base->nr_threads + 1);
	pthread_barrier_init(&base->work_done_barrier, NULL, base->nr_threads + 1);
#if TAS_INFO == 1
	printf("tas_abstract_init -- Barriers are initialized now.\n");
#endif
#else
	barrier_init(&base->work_available_barrier, base->nr_threads + 1);
	barrier_init(&base->work_done_barrier, base->nr_threads + 1);
#endif

	// If there are workers: Start threads which immediately wait at work_done_barrier
	// TODO: Ensure that there are threads available and running the code...

	if ((TAS_MAIN_IS_WORKER && base->nr_threads > 0)
			|| (!TAS_MAIN_IS_WORKER && base->nr_threads > 1)) {

		int next_worker = -1;
		for (i = 0; i < base->nr_threads && (next_worker = get_next_worker(threads, next_worker)) >= 0; i++) {
			int worker_id = tas_worker_get(next_worker);
			base->workers[i] = worker_id;

			barrier_wait(
					&(worker_threads_data[worker_id].pattern_assigned_barrier_idle)); // ID=tas_abstract_init_pattern_assigned_barrier_idle

			// Assign pattern to the thread's data structure
			worker_threads_data[worker_id].parent = base;
			worker_threads_data[worker_id].pattern_assigned = 1;

#if TAS_INFO == 1
			PFL
			printf("Assigned worker %i to slot %i of %i of the pattern\n",
					worker_id, i,
					worker_threads_data[worker_id].parent->nr_threads);
			PFU
#endif

			barrier_wait(
					&(worker_threads_data[worker_id].pattern_assigned_barrier)); // ID=tas_abstract_init_pattern_assigned_barrier

#if TAS_DEBUG == 1
			PFL
			printf(
					"Notified worker %i in slot %i of the pattern that it is assigned (pattern_assigned_barrier)\n",
					worker_id, i);
			PFU
#endif
		}
	}

#ifdef TAS_STATISTICS
	logEnd("tas_abstract_init");
#endif

#if TAS_INFO == 1
	PFL
#if TAS_POSIX == 1
	printf("END tas_abstract_init with %i + %i threads based on 0x%" PRIX64 " as workers\n",
			base->nr_threads, TAS_MAIN_IS_WORKER, threads);
#else
	printf("END tas_abstract_init with %i + %i threads based on %u as workers\n",
			base->nr_threads, TAS_MAIN_IS_WORKER, threads);
#endif
	PFU
#endif
}

void tas_abstract_finalize(tas_abstract_t * base) {
	int i;

#ifdef TAS_STATISTICS
	logStart("tas_abstract_finalize");
#endif

	for (i = 0; i < base->nr_threads; i++) {
		worker_threads_data[base->workers[i]].work_assigned = 0;
	}

	if ((TAS_MAIN_IS_WORKER && base->nr_threads > 0)
			|| (!TAS_MAIN_IS_WORKER && base->nr_threads > 1)) {
		// join barrier_work_available
		barrier_wait(&base->work_available_barrier); // ID=tas_abstract_finalize_work_available_barrier

#if TAS_INFO == 1
		PFL
		printf("MAIN - Joined work_available_barrier\n");
		PFU
#endif

		for (i = 0; i < base->nr_threads; i++) {
			worker_threads_data[base->workers[i]].pattern_assigned = 0;
			// worker_threads_data[base->workers[i]].parent = NULL;
		}

		barrier_wait(&base->work_done_barrier); // ID=tas_abstract_finalize_work_done_barrier

#if TAS_INFO == 1
		PFL
		printf("MAIN - Joined work_done_barrier\n");
		PFU
#endif

		// join barrier_work_done
		/* #if TAS_POSIX == 1
		 pthread_barrier_wait(&base->work_done_barrier);
		 #else
		 barrier_wait(&base->work_done_barrier, &base->nr_threads + 1);
		 #endif */

		// Release the workers
		for (i = 0; i < base->nr_threads; i++) {
			int worker_id = base->workers[i];
			tas_worker_release(worker_id);

#if TAS_INFO == 1
			PFL
			printf("Released worker %i from slot %i of the pattern\n",
					worker_id, i);
			PFU
#endif
		}

#if TAS_INFO == 1
		PFL
		printf("MAIN - Released all the workers.\n");
		PFU
#endif
	}

#ifdef TAS_STATISTICS
	logEnd("tas_abstract_finalize");
#endif
}

void tas_taskparallel_init(tas_taskparallel_t * instance, uint64_t worker_ids) {
	tas_abstract_init(&instance->base, worker_ids);
}

void tas_taskparallel_finalize(tas_taskparallel_t * instance) {
	tas_abstract_finalize(&instance->base);
}

void tas_dataparallel_init(tas_dataparallel_t * instance, uint64_t worker_ids) {
	tas_abstract_init(&instance->base, worker_ids);
}

void tas_dataparallel_finalize(tas_dataparallel_t * instance) {
	tas_abstract_finalize(&instance->base);
}


/** Execute task parallel pattern */
void tas_taskparallel_execute_with_limits(tas_taskparallel_t * instance, int first, int last) {
#ifdef TAS_STATISTICS
	long duration;
	logStart("Task Parallel");
#endif

	if ((TAS_MAIN_IS_WORKER && instance->base.nr_threads == 0)
			|| (!TAS_MAIN_IS_WORKER && instance->base.nr_threads == 1)) {
#if TAS_DEBUG == 1
		PFL
		printf("#### Starting sequential implementation for %i elements\n",
				instance->nr_runnables);
		PFU
#endif
		int i = 0;
		for (i = 0; i < instance->nr_runnables; i++) {
			// printf("dp b %i\n", i);
			if(first <= i && i <= last) {
				tas_runnable_t foo = instance->runnables[i];
				(*foo)(instance->args[i]);
			}
		}
	} else {
		int i, round;

		int elements_per_round = instance->base.nr_threads + TAS_MAIN_IS_WORKER;
		int nr_workers = instance->base.nr_threads;
		int total_elements = instance->nr_runnables;

		int rounds = total_elements / elements_per_round;
		if (total_elements % elements_per_round > 0)
			rounds++;

#if TAS_INFO == 1
		PFL
		printf("#### Starting %i elements with %i+%i threads => %i rounds\n",
				instance->nr_runnables, instance->base.nr_threads,
				TAS_MAIN_IS_WORKER, rounds);
		PFU
#endif

		for (round = 0; round < rounds; round++) { // ID=tas_taskparallel_execute_loop
#if TAS_INFO == 1
			PFL
			printf("# Round %i (%i per round, %i elements in total)\n", round,
					elements_per_round, total_elements);
			PFU
#endif

			// Assign work to threads if available
			for (i = 0;
					i < nr_workers
							&& round * elements_per_round + i < total_elements;
					i++) {
				if(first <= (round * elements_per_round + i) && (round * elements_per_round + i) <= last) {
#if TAS_DEBUG == 1
					PFL
					printf(" - Round %i: thread %i gets element %i\n", round, i,
							round * elements_per_round + i);
					PFU
#endif
					worker_threads_data[instance->base.workers[i]].runnable =
							instance->runnables[round * elements_per_round + i];
					worker_threads_data[instance->base.workers[i]].args =
							instance->args[round * elements_per_round + i];
					worker_threads_data[instance->base.workers[i]].work_assigned =
							1;
				} else {
#if TAS_DEBUG == 1
					PFL
					printf(" - Round %i: thread %i is on standby, %i not in [%i, %i]\n", round, i,
							round * elements_per_round + i, first, last);
					PFU
#endif
				}
			}

			// join barrier_work_available
			barrier_wait(&instance->base.work_available_barrier); // ID=tas_taskparallel_execute_work_available_barrier

			if (TAS_MAIN_IS_WORKER) {
				int element_for_main = round * elements_per_round
						+ elements_per_round - 1;
				if (element_for_main < total_elements) {
					if(first <= element_for_main && element_for_main <= last) {
#if TAS_DEBUG == 1
						PFL
						printf(" - Round %i: MAIN gets element %i\n", round,
								element_for_main);
						PFU
#endif
						tas_runnable_t foo = instance->runnables[element_for_main];
						(*foo)(instance->args[element_for_main]);
					} else {
						PFL
						printf(" - Round %i: No element left for main, %i not in [%i, %i].\n", round, element_for_main, first, last);
						PFU
					}
				} else {
					PFL
					printf(" - Round %i: No element left for main.\n", round);
					PFU
				}
			}

			// join barrier_work_done
			barrier_wait(&instance->base.work_done_barrier); // ID=tas_taskparallel_execute_work_done_barrier
		}

	}

#ifdef TAS_STATISTICS
	duration = logEnd("Task Parallel");
	printf("DATA DataParallel %i runnables %i threads %ld time\n",
			instance->nr_runnables, instance->base.nr_threads + TAS_MAIN_IS_WORKER, duration);
#endif
}

void tas_pipeline_execute(tas_taskparallel_t * task_parallelism, int iterations, tas_pipeline_inout_t * io) {
	int i;
	int inIndex = 0;
	int outIndex = 1;
	int temp_index;

	for (i = 0; i < iterations + task_parallelism->nr_runnables - 1; i++) { // ID=tas_pipeline_execute_loop
		printf("Generating input data in %i for a and %i for b\n", inIndex, outIndex);

		int first_task = TAS_MAX(0, i - iterations + 1);
		int last_task = TAS_MIN(i, task_parallelism->nr_runnables - 1);

		printf("Running pipeline with stages %i to %i of %i\n", first_task, last_task, task_parallelism->nr_runnables);
		tas_taskparallel_execute_with_limits(task_parallelism, first_task, last_task); // ID=tas_pipeline_tp_execute

		void * temp = io->output;
		io->output = io->input;
		io->input = temp;

		temp_index = inIndex;
		inIndex = outIndex;
		outIndex = temp_index;

		printf("Completed run %i / %i\n", i + 1, iterations + task_parallelism->nr_runnables - 1);
	}
}

/** Execute task parallel pattern */
void tas_taskparallel_execute(tas_taskparallel_t * instance) {
	tas_taskparallel_execute_with_limits(instance, 0, instance->nr_runnables);
}

/** Execute data parallel pattern */
void tas_dataparallel_execute(tas_dataparallel_t * instance) {
#ifdef TAS_STATISTICS
	long duration;
	logStart("Data Parallel");
#endif

	if ((TAS_MAIN_IS_WORKER && instance->base.nr_threads == 0)
			|| (!TAS_MAIN_IS_WORKER && instance->base.nr_threads == 1)) {
#if TAS_INFO == 1
		PFL
		printf("#### Starting sequential implementation for %i elements\n",
				instance->nr_args);
		PFU
#endif
		int i = 0;
		for (i = 0; i < instance->nr_args; i++) {
			// printf("dp b %i\n", i);
			tas_runnable_t foo = instance->runnable;
			(*foo)(instance->args[i]);
		}
	} else { // if(instance->nr_args <= instance->base.nr_threads) {
		int i, round;

		int elements_per_round = instance->base.nr_threads + TAS_MAIN_IS_WORKER;
		int nr_workers = instance->base.nr_threads;
		int total_elements = instance->nr_args;

		int rounds = total_elements / elements_per_round;
		if (total_elements % elements_per_round > 0)
			rounds++;

#if TAS_INFO == 1
		PFL
		printf("#### Starting %i elements with %i+%i threads => %i rounds\n",
				instance->nr_args, instance->base.nr_threads,
				TAS_MAIN_IS_WORKER, rounds);
		PFU
#endif

		for (round = 0; round < rounds; round++) { // ID=tas_dataparallel_execute_loop
#if TAS_INFO == 1
			PFL
			printf("# Round %i (%i per round, %i elements in total)\n", round,
					elements_per_round, total_elements);
			PFU
#endif

			// Assign work to threads if available
			for (i = 0;
					i < nr_workers
							&& round * elements_per_round + i < total_elements;
					i++) {
#if TAS_DEBUG == 1
				PFL
				printf(" - Round %i: thread %i gets element %i\n", round, i,
						round * elements_per_round + i);
				PFU
#endif
				worker_threads_data[instance->base.workers[i]].runnable =
						instance->runnable;
				worker_threads_data[instance->base.workers[i]].args =
						instance->args[round * elements_per_round + i];
				worker_threads_data[instance->base.workers[i]].work_assigned =
						1;
			}

			// join barrier_work_available
			barrier_wait(&instance->base.work_available_barrier); // ID=tas_dataparallel_execute_work_available_barrier

			if (TAS_MAIN_IS_WORKER) {
				int element_for_main = round * elements_per_round
						+ elements_per_round - 1;
				if (element_for_main < total_elements) {
#if TAS_DEBUG == 1
					PFL
					printf(" - Round %i: MAIN gets element %i\n", round,
							element_for_main);
					PFU
#endif
					tas_runnable_t foo = instance->runnable;
					(*foo)(instance->args[element_for_main]);
				}
			}

			// join barrier_work_done
			barrier_wait(&instance->base.work_done_barrier); // ID=tas_dataparallel_execute_work_done_barrier
		}

	}

#ifdef TAS_STATISTICS
	duration = logEnd("Data Parallel");
	printf("DATA DataParallel %i elements %i threads %ld time\n",
			instance->nr_args, instance->base.nr_threads + TAS_MAIN_IS_WORKER, duration);
#endif
}
