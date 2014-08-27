
/**
 * Initialize the TAS thread pool -> create threads
 * @param workers Number of workers
 * */
void tas_init(int workers) {
	int i;

	logStart("tas_init");

	worker_total_count = workers;

	barrier_init(&worker_init_barrier, workers + 1);
	ticket_init(&worker_available_lock);

	printf("MAIN - START tas_init with %i workers\n", workers);

	// Initialize structures for threads
	for (i = 0; i < worker_total_count; i++) {
		worker_threads_data[i].do_shutdown = 0;
		worker_threads_data[i].work_assigned = 0;
		worker_threads_data[i].pattern_assigned = 0;
		worker_threads_data[i].thread_id = i;

		// Initialize barriers (n + 1)
		barrier_init(&(worker_threads_data[i].pattern_assigned_barrier), 2);
		barrier_init(&(worker_threads_data[i].pattern_assigned_barrier_idle), 2);
	}

	printf("MAIN - END tas_init\n");

	logEnd("tas_init");
}


void tas_abstract_init(tas_abstract_t * base, int threads) {
	int i;

	printf("START tas_abstract_init with %i threads\n", threads);

	if (tas_abstract_init_first) {
		printf("MAIN - Wait for workers... (worker_init_barrier) \n");

		barrier_wait(&worker_init_barrier); // ID=tas_abstract_init_worker_init_barrier

		printf("MAIN - Passed worker_init_barrier.\n");

		tas_abstract_init_first = 0;
	}

	if (TAS_MAIN_IS_WORKER)
		base->nr_threads = threads - 1;
	else
		base->nr_threads = threads;

	if (base->nr_threads > TAS_MAX_WORKERS)
		base->nr_threads = TAS_MAX_WORKERS;

	// Initialize barriers (n + 1) of pattern
	barrier_init(&base->work_available_barrier, base->nr_threads + 1);
	barrier_init(&base->work_done_barrier, base->nr_threads + 1);

	// If there are workers: Start threads which immediately wait at work_done_barrier
	// TODO: Ensure that there are threads available and running the code...

	if ((TAS_MAIN_IS_WORKER && base->nr_threads > 0)
			|| (!TAS_MAIN_IS_WORKER && base->nr_threads > 1)) {

		for (i = 0; i < base->nr_threads; i++) {
			int worker_id = tas_worker_get();
			base->workers[i] = worker_id;

			barrier_wait(&(worker_threads_data[worker_id].pattern_assigned_barrier_idle)); // ID=tas_abstract_init_pattern_assigned_barrier_idle

			// Assign pattern to the thread's data structure
			worker_threads_data[worker_id].parent = base;
			worker_threads_data[worker_id].pattern_assigned = 1;

			printf("Assigned worker %i to slot %i of %i of the pattern\n",
					worker_id, i,
					worker_threads_data[worker_id].parent->nr_threads);

			barrier_wait(&(worker_threads_data[worker_id].pattern_assigned_barrier)); // ID=tas_abstract_init_pattern_assigned_barrier

			printf(
					"Notified worker %i in slot %i of the pattern that it is assigned (pattern_assigned_barrier)\n",
					worker_id, i);
		}
	}

	printf("END tas_abstract_init with %i == %i + %i threads\n", threads,
			base->nr_threads, TAS_MAIN_IS_WORKER);
}

void tas_abstract_finalize(tas_abstract_t * base) {
	int i;

	for (i = 0; i < base->nr_threads; i++) {
		worker_threads_data[base->workers[i]].work_assigned = 0;
	}

	if ((TAS_MAIN_IS_WORKER && base->nr_threads > 0)
			|| (!TAS_MAIN_IS_WORKER && base->nr_threads > 1)) {
		// join barrier_work_available
		barrier_wait(&base->work_available_barrier); // ID=tas_abstract_finalize_work_available_barrier

		printf("MAIN - Joined work_available_barrier\n");

		for (i = 0; i < base->nr_threads; i++) {
			worker_threads_data[base->workers[i]].pattern_assigned = 0;
		}

		barrier_wait(&base->work_done_barrier); // ID=tas_abstract_finalize_work_done_barrier

		printf("MAIN - Joined work_done_barrier\n");

		// Release the workers
		for (i = 0; i < base->nr_threads; i++) {
			int worker_id = base->workers[i];
			tas_worker_release(worker_id);

			printf("Released worker %i from slot %i of the pattern\n",
					worker_id, i);
		}
		printf("MAIN - Released all the workers.\n");
	}
}


/** Code to be executed by all the worker threads */
void * tas_thread(void * parameters_void) {
	tas_thread_t * param = (tas_thread_t *) parameters_void;

	printf("T%i A - Starting on core %i...\n", param->thread_id, procnum());

	// Register this worker
	ticket_lock(&worker_available_lock); // ID=tas_thread_worker_available_lock
	worker_available_nr++;
	worker_available[param->thread_id] = 1;

	printf("T%i A - Now are %i workers available.\n", param->thread_id, worker_available_nr);

	ticket_unlock(&worker_available_lock); // ID=tas_thread_worker_available_lock

	printf("T%i A - Waiting for other workers and main to reach worker_init_barrier...\n", param->thread_id);
	barrier_wait(&worker_init_barrier); // ID=tas_thread_worker_init_barrier
	printf("T%i A - Passed worker_init_barrier.\n", param->thread_id);

	// Wait for something to happen...
	while (1) { // ID=loop_runnable
		printf("T%i - NEXT ITERATION.\n", param->thread_id);

		// Wait for pattern to be assigned
		if (!param->pattern_assigned) {
			printf("T%i A - waits for a pattern (pattern_assigned_barrier_idle and pattern_assigned_barrier).\n", param->thread_id);
			barrier_wait(&param->pattern_assigned_barrier_idle); // ID=tas_thread_pattern_assigned_barrier_idle
			barrier_wait(&param->pattern_assigned_barrier); // ID=tas_thread_pattern_assigned_barrier
			printf("T%i A - passed pattern_assigned_barrier.\n", param->thread_id);

			// Check if thread shall exit
			if (param->do_shutdown) {
				printf("T%i D - is shutting down.\n", param->thread_id);
				break;
			} else {
				continue;
			}
		}

		// Pattern is assigned => wait for work
		if (param->pattern_assigned) {
			printf(
					"T%i B - is assigned to a pattern (%i threads) and waits for work (work_available_barrier).\n",
					param->thread_id,
					worker_threads_data[param->thread_id].parent->nr_threads);

			// Wait for work to be available
			barrier_wait(&(worker_threads_data[param->thread_id].parent->work_available_barrier)); // ID=tas_thread_work_available_barrier

			printf("T%i B - passed work_available_barrier.\n",
					param->thread_id);

			// Do work
			if (param->work_assigned) {
				printf("T%i C - got work.\n", param->thread_id);

				tas_runnable_t foo = param->runnable;
				(*foo)(param->args);

				// Reset thread
				param->work_assigned = 0;

				printf("T%i C - finished working.\n", param->thread_id);
			} else {
				printf("T%i C - did not get work assigned.\n",
						param->thread_id);
			}

			// Wait for work done
			printf("T%i C - waits for work_done_barrier (%i threads)....\n",
					param->thread_id,
					worker_threads_data[param->thread_id].parent->nr_threads + 1);
			
			barrier_wait(&(worker_threads_data[param->thread_id].parent->work_done_barrier)); // ID=tas_thread_work_done_barrier
			
			printf("T%i C - passed work_done_barrier.\n", param->thread_id);

			continue;
		}
	}

	printf("T%i D - is exiting. Good bye!\n", param->thread_id);

	return NULL;
}


/** Execute task parallel pattern */
void tas_taskparallel_execute_with_limits(tas_taskparallel_t * instance, int first, int last) {
	if ((TAS_MAIN_IS_WORKER && instance->base.nr_threads == 0)
			|| (!TAS_MAIN_IS_WORKER && instance->base.nr_threads == 1)) {
		printf("#### Starting sequential implementation for %i elements\n",
				instance->nr_runnables);

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

		printf("#### Starting %i elements with %i+%i threads => %i rounds\n",
				instance->nr_runnables, instance->base.nr_threads,
				TAS_MAIN_IS_WORKER, rounds);

		for (round = 0; round < rounds; round++) { // ID=tas_taskparallel_execute_loop
			printf("# Round %i (%i per round, %i elements in total)\n", round,
					elements_per_round, total_elements);


			// Assign work to threads if available
			for (
					i = 0;
					i < nr_workers && round * elements_per_round + i < total_elements;
					i++
			) {
				if(first <= (round * elements_per_round + i) && (round * elements_per_round + i) <= last) {
					printf(" - Round %i: thread %i gets element %i\n", round, i,
							round * elements_per_round + i);

					worker_threads_data[instance->base.workers[i]].runnable =
							instance->runnables[round * elements_per_round + i];
					worker_threads_data[instance->base.workers[i]].args =
							instance->args[round * elements_per_round + i];
					worker_threads_data[instance->base.workers[i]].work_assigned =
							1;
				} else {
					printf(" - Round %i: thread %i is on standby, %i not in [%i, %i]\n", round, i,
							round * elements_per_round + i, first, last);

				}
			}

			// join barrier_work_available
			barrier_wait(&instance->base.work_available_barrier); // ID=tas_taskparallel_execute_work_available_barrier

			if (TAS_MAIN_IS_WORKER) {
				int element_for_main = round * elements_per_round
						+ elements_per_round - 1;
				if (element_for_main < total_elements) {
					if(first <= element_for_main && element_for_main <= last) {
						printf(" - Round %i: MAIN gets element %i\n", round,
								element_for_main);

						tas_runnable_t foo = instance->runnables[element_for_main];
						(*foo)(instance->args[element_for_main]);
					} else {
						PFL
						printf(" - Round %i: No element left for main, %i not in [%i, %i].", round, element_for_main, first, last);
						PFU
					}
				} else {
					PFL
					printf(" - Round %i: No element left for main.", round);
					PFU
				}
			}

			// join barrier_work_done
			barrier_wait(&instance->base.work_done_barrier); // ID=tas_taskparallel_execute_work_done_barrier
		}

	}
}