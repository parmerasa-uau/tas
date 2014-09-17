#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "tas_demo_7.h"
#include "data.h"
// #include "skel_mw/skel_manager_worker.h"
#include "tas.h"
#include "tas_log.h"

#include "../platform.h"

// #define DEBUG_LAPLACE 0

double world_temp[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z];

struct thread_data_jacobi {
	int thread_id;
	int offset; // Offset where to start
	int partsize; // Number of layers to process
	int iterations; // Only for version with barrier
	double (*ptr_from)[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z];
	double (*ptr_to)[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z];
	int do_shutdown;
};

void init_copy_world() {
	int a, b, c;

	logStart("initialize copy-weightmap from obstacle map");

	for (a = 0; a < WORLD_SIZE_X; a++) {
		for (b = 0; b < WORLD_SIZE_Y; b++) {
			for (c = 0; c < WORLD_SIZE_Z; c++) {
				if (isGoal(a, b, c)) {
					world_temp[a][b][c] = WEIGHT_SINK;
				} else if (isObstacle(a, b, c)) {
					world_temp[a][b][c] = WEIGHT_OBSTACLE;
				} else {
					// world[a][b][c] = WEIGHT_INIT;
				}
			}
		}
	}

	logEnd("initialize copy-weightmap from obstacle map");
}

// IMPLEMENTATION WITHOUT BARRIER

void * algo_jacobi_par_runnable(void * threadarg) {
	struct thread_data_jacobi * my_data;
	my_data = (struct thread_data_jacobi *) threadarg;
	int a, b, c;

	// printf("T%i Task  initialized (%i, %i)...\n", my_data->thread_id, my_data->offset, my_data->partsize);

	for (a = my_data->offset; a < my_data->offset + my_data->partsize; a++) {
		for (b = 0; b < WORLD_SIZE_Y; b++) {
			for (c = 0; c < WORLD_SIZE_Z; c++) {
				if (isGoal(a, b, c)) {
					// (*my_data->ptr_from)[a][b][c] = WEIGHT_SINK;
					// (*my_data->ptr_to)[a][b][c] = WEIGHT_SINK;
				} else if (isObstacle(a, b, c)) {
					// (*my_data->ptr_from)[a][b][c] = WEIGHT_OBSTACLE;
					// (*my_data->ptr_to)[a][b][c] = WEIGHT_OBSTACLE;
				} else {
					(*my_data->ptr_to)[a][b][c] = getAverage(
							*(my_data->ptr_from), a, b, c);
				}
			}
		}
	}

	// printf("T%i Task finished.\n", my_data->thread_id);

	return NULL ;
}

void algo_jacobi_par(int iterations, int threads) {
	int i, j;

	logStart("promote_world algo_jacobi_par");

	if (WORLD_SIZE_X % threads > 0) {
		printf(
				"WORLD_SIZE_X [%i] must be a multiple of THREADS [%i], which is not given. => FATAL ERROR!\n",
				WORLD_SIZE_X, threads);
		exit(1);
	}

	// Initialize copy-world
	init_copy_world();

	// Variables for thread
	pthread_t thread_tasks[threads];
	struct thread_data_jacobi thread_data_tasks[threads];

	// Assign thread ids
	for (i = 0; i < threads; i++) {
		thread_data_tasks[i].thread_id = i;
	}

	// Split the data
	for (i = 0; i < threads; i++) {
		thread_data_tasks[i].offset = (WORLD_SIZE_X / threads) * i;
		thread_data_tasks[i].partsize = WORLD_SIZE_X / threads;
	}

	// Run threads
	for (j = 0; j < iterations; j++) {
		// Toggle to and from matrix
		for (i = 0; i < threads; i++) {
			if (j % 2 == 0) {
				thread_data_tasks[i].ptr_from = &world;
				thread_data_tasks[i].ptr_to = &world_temp;
			} else {
				thread_data_tasks[i].ptr_from = &world_temp;
				thread_data_tasks[i].ptr_to = &world;
			}
		}

		// Start threads
		for (i = 0; i < threads; i++) {
			pthread_create(&thread_tasks[i], NULL, algo_jacobi_par_runnable,
					(void *) &thread_data_tasks[i]);
		}

		// Join threads
		for (i = 0; i < threads; i++)
			pthread_join(thread_tasks[i], NULL );
	}

	logEnd("promote_world algo_jacobi_par");
}

// IMPLEMENTATION WITH BARRIER

pthread_barrier_t barr;

void algo_jacobi_par_barrier_runnable(void * threadarg) {
	struct thread_data_jacobi * my_data;
	my_data = (struct thread_data_jacobi *) threadarg;
	int i, a, b, c;

#ifdef DEBUG_LAPLACE
	printf("T%i Task initialized (%i, %i)...\n", my_data->thread_id, my_data->offset, my_data->partsize);
#endif

	for (i = 0; i < my_data->iterations / 2; i++) {
		int rc;

		for (a = my_data->offset; a < my_data->offset + my_data->partsize;
				a++) {
			for (b = 0; b < WORLD_SIZE_Y; b++) {
				for (c = 0; c < WORLD_SIZE_Z; c++) {
					if (isGoal(a, b, c)) {
						// (*my_data->ptr_from)[a][b][c] = WEIGHT_SINK;
						// (*my_data->ptr_to)[a][b][c] = WEIGHT_SINK;
					} else if (isObstacle(a, b, c)) {
						// (*my_data->ptr_from)[a][b][c] = WEIGHT_OBSTACLE;
						// (*my_data->ptr_to)[a][b][c] = WEIGHT_OBSTACLE;
					} else {
						(*my_data->ptr_to)[a][b][c] = getAverage(
								*(my_data->ptr_from), a, b, c);
					}
				}
			}
		}

		// Synchronization point
		rc = pthread_barrier_wait(&barr);
		if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
			printf("Could not wait on barrier\n");
			exit(-1);
		}

		// printf("T%i Task completed iteration A-%i...\n", my_data->thread_id, i);

		for (a = my_data->offset; a < my_data->offset + my_data->partsize;
				a++) {
			for (b = 0; b < WORLD_SIZE_Y; b++) {
				for (c = 0; c < WORLD_SIZE_Z; c++) {
					if (isGoal(a, b, c)) {
						// (*my_data->ptr_from)[a][b][c] = WEIGHT_SINK;
						// (*my_data->ptr_to)[a][b][c] = WEIGHT_SINK;
					} else if (isObstacle(a, b, c)) {
						// (*my_data->ptr_from)[a][b][c] = WEIGHT_OBSTACLE;
						// (*my_data->ptr_to)[a][b][c] = WEIGHT_OBSTACLE;
					} else {
						(*my_data->ptr_from)[a][b][c] = getAverage(
								*(my_data->ptr_to), a, b, c);
					}
				}
			}
		}

		// Synchronization point
		rc = pthread_barrier_wait(&barr);
		if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
			printf("Could not wait on barrier\n");
			exit(-1);
		}

#ifdef DEBUG_LAPLACE
		printf("T%i Task completed iteration B-%i...\n", my_data->thread_id, i);
#endif
	}

#ifdef DEBUG_LAPLACE
	printf("T%i Task finished.\n", my_data->thread_id);
#endif

	// return NULL ;
}


/* void algo_jacobi_par_barrier(int iterations) {
	int i, j;

	logStart("promote_world algo_jacobi_par_barrier");

	if (WORLD_SIZE_X % THREADS > 0) {
		printf(
				"WORLD_SIZE_X [%i] must be a multiple of THREADS [%i], which is not given. => FATAL ERROR!\n",
				WORLD_SIZE_X, THREADS);
		exit(1);
	}

	// Initialize copy-world
	init_copy_world();

	// Variables for thread
	pthread_t thread_tasks[THREADS];
	struct thread_data_jacobi thread_data_tasks[THREADS];

	// Assign thread ids
	for (i = 0; i < THREADS; i++) {
		thread_data_tasks[i].thread_id = i;
	}

	// Barrier initialization
	if (pthread_barrier_init(&barr, NULL, THREADS)) {
		printf("Could not create a barrier\n");
		exit(1);
	}

	// Split the data
	for (i = 0; i < THREADS; i++) {
		thread_data_tasks[i].offset = (WORLD_SIZE_X / THREADS) * i;
		thread_data_tasks[i].partsize = WORLD_SIZE_X / THREADS;
		thread_data_tasks[i].ptr_from = &world;
		thread_data_tasks[i].ptr_to = &world_temp;
		thread_data_tasks[i].iterations = iterations;
	}

	// Start threads
	for (i = 0; i < THREADS; i++) {
		pthread_create(&thread_tasks[i], NULL, algo_jacobi_par_barrier_runnable,
				(void *) &thread_data_tasks[i]);
	}

	// Join threads
	for (i = 0; i < THREADS; i++)
		pthread_join(thread_tasks[i], NULL );

	logEnd("promote_world algo_jacobi_par_barrier");
} */

pthread_barrier_t work_available_barrier;
pthread_barrier_t work_done_barrier;

void * algo_jacobi_par_runnable_2(void * threadarg) {
	struct thread_data_jacobi * my_data;
	my_data = (struct thread_data_jacobi *) threadarg;
	int a, b, c;

	// if(DEBUG) printf("T%i Thread started (%i, %i)...\n", my_data->thread_id, my_data->offset, my_data->partsize);


	while (1) {
		// Wait for work to be available
		pthread_barrier_wait(&work_available_barrier);

		// Do work
		if(!my_data->do_shutdown) {
			for (a = my_data->offset; a < my_data->offset + my_data->partsize;
					a++) {
				for (b = 0; b < WORLD_SIZE_Y; b++) {
					for (c = 0; c < WORLD_SIZE_Z; c++) {
						if (isGoal(a, b, c)) {
							// (*my_data->ptr_from)[a][b][c] = WEIGHT_SINK;
							// (*my_data->ptr_to)[a][b][c] = WEIGHT_SINK;
						} else if (isObstacle(a, b, c)) {
							// (*my_data->ptr_from)[a][b][c] = WEIGHT_OBSTACLE;
							// (*my_data->ptr_to)[a][b][c] = WEIGHT_OBSTACLE;
						} else {
							(*my_data->ptr_to)[a][b][c] = getAverage(
									*(my_data->ptr_from), a, b, c);
						}
					}
				}
			}

			// if(DEBUG) printf("T%i Thread has finished %i...\n", my_data->thread_id, my_data->iteration);
		} else {
			// if(DEBUG) printf("T%i Thread is now in shutdown mode.\n", my_data->thread_id);
			break;
		}

		// Wait for work to be available
		pthread_barrier_wait(&work_done_barrier);
	}

	// if(DEBUG) printf("T%i Thread terminated.\n", my_data->thread_id);

	return NULL ;
}

void algo_jacobi_par_barrier_2(int iterations, int threads) {
	int i, j;

	logStart("promote_world algo_jacobi_par");

	if (WORLD_SIZE_X % THREADS > 0) {
		printf(
				"WORLD_SIZE_X [%i] must be a multiple of THREADS [%i], which is not given. => FATAL ERROR!\n",
				WORLD_SIZE_X, threads);
		exit(1);
	}

	// Initialize copy-world
	init_copy_world();

	// Variables for thread
	pthread_t thread_tasks[threads];
	struct thread_data_jacobi thread_data_tasks[threads];

	// Assign thread ids
	for (i = 0; i < threads; i++) {
		thread_data_tasks[i].thread_id = i;
		thread_data_tasks[i].do_shutdown = 0;
	}

	// Split the data
	for (i = 0; i < threads; i++) {
		thread_data_tasks[i].offset = (WORLD_SIZE_X / threads) * i;
		thread_data_tasks[i].partsize = WORLD_SIZE_X / threads;
	}

	// Barrier initialization (Threads + main)
	pthread_barrier_init(&work_available_barrier, NULL, threads + 1);
	pthread_barrier_init(&work_done_barrier, NULL, threads + 1);

	// Start threads
	for (i = 0; i < threads; i++) {
		pthread_create(&thread_tasks[i], NULL, algo_jacobi_par_runnable_2,
				(void *) &thread_data_tasks[i]);
	}

	// Run threads
	for (j = 0; j < iterations; j++) {
		// Toggle to and from matrix
		for (i = 0; i < threads; i++) {
			if (j % 2 == 0) {
				thread_data_tasks[i].ptr_from = &world;
				thread_data_tasks[i].ptr_to = &world_temp;
			} else {
				thread_data_tasks[i].ptr_from = &world_temp;
				thread_data_tasks[i].ptr_to = &world;
			}
		}

		// Wait for other threads to start working
		pthread_barrier_wait(&work_available_barrier);

		// Wait for work to be finished
		pthread_barrier_wait(&work_done_barrier);
	}

	// Prepare exit...
	for (i = 0; i < threads; i++) {
		thread_data_tasks[i].do_shutdown = 1;
	}

	// Wait for other threads to proces poison
	pthread_barrier_wait(&work_available_barrier);

	// Join threads
	for (i = 0; i < threads; i++)
		pthread_join(thread_tasks[i], NULL );

	logEnd("promote_world algo_jacobi_par");

}

// IMPLEMENTATION WITH SKELETONS AND LOOP WITH SYNC IN RUNNABLES

void algo_jacobi_par_skel_complex(int iterations, int threads) {
	int i;
	long duration;

	logStart("promote_world algo_jacobi_par_skel_complex");

	if (WORLD_SIZE_X % threads > 0) {
		printf(
				"WORLD_SIZE_X [%i] must be a multiple of THREADS [%i], which is not given. => FATAL ERROR!\n",
				WORLD_SIZE_X, threads);
		exit(1);
	}

	// Initialize copy-world
	init_copy_world();

	// Variables for thread
	struct thread_data_jacobi thread_data_tasks[threads];

	// Assign thread ids
	for (i = 0; i < threads; i++) {
		thread_data_tasks[i].thread_id = i;
	}

	// Barrier initialization
	if (pthread_barrier_init(&barr, NULL, threads)) {
		printf("Could not create a barrier\n");
		exit(1);
	}

	// Split the data
	for (i = 0; i < threads; i++) {
		thread_data_tasks[i].offset = (WORLD_SIZE_X / threads) * i;
		thread_data_tasks[i].partsize = WORLD_SIZE_X / threads;
		thread_data_tasks[i].ptr_from = &world;
		thread_data_tasks[i].ptr_to = &world_temp;
		thread_data_tasks[i].iterations = iterations;
	}

	void * args[threads];
	for (i = 0; i < threads; i++) {
		args[i] = & (thread_data_tasks[i]);
	}

	tas_runnable_t runnable = &algo_jacobi_par_barrier_runnable;

	tas_dataparallel_t dp = { runnable, args, threads }; // runnable, arguments, number of arguments
	tas_dataparallel_init(&dp, threads);
	tas_dataparallel_execute(&dp);
	tas_dataparallel_finalize(&dp);

	duration = logEnd("promote_world algo_jacobi_par_skel_complex");

#if TAS_POSIX == 1
	printf("DATA algo_jacobi_par_skel_complex iterations %i threads %i duration %lu\n", iterations, threads, duration);
#else
	printf("DATA algo_jacobi_par_skel_complex iterations %i threads %i duration %i\n", iterations, threads, duration);
#endif
}


// IMPLEMENTATION WITH SKELETONS AND NO SYNC IN RUNNABLES

void  algo_jacobi_par_skel_simple_runnable(void * threadarg) {
	struct thread_data_jacobi * my_data;
	my_data = (struct thread_data_jacobi *) threadarg;
	int a, b, c;

#ifdef DEBUG_LAPLACE
	printf("T%i Task initialized (%i, %i)...\n", my_data->thread_id, my_data->offset, my_data->partsize);
#endif

	for (a = my_data->offset; a < my_data->offset + my_data->partsize; a++) {
		for (b = 0; b < WORLD_SIZE_Y; b++) {
			for (c = 0; c < WORLD_SIZE_Z; c++) {
				if (isGoal(a, b, c)) {
					// (*my_data->ptr_from)[a][b][c] = WEIGHT_SINK;
					// (*my_data->ptr_to)[a][b][c] = WEIGHT_SINK;
				} else if (isObstacle(a, b, c)) {
					// (*my_data->ptr_from)[a][b][c] = WEIGHT_OBSTACLE;
					// (*my_data->ptr_to)[a][b][c] = WEIGHT_OBSTACLE;
				} else {
					(*my_data->ptr_to)[a][b][c] = getAverage(
							*(my_data->ptr_from), a, b, c);
				}
			}
		}
	}

#ifdef DEBUG_LAPLACE
	printf("T%i Task finished.\n", my_data->thread_id);
#endif

	// return NULL ;
}

// Variables for thread
struct thread_data_jacobi thread_data_tasks[TOTAL_PROC_NUM];

void algo_jacobi_par_skel_simple(int iterations, int foo) {
	int i, j;
	long duration;
	int threads = foo;

	logStart("promote_world algo_jacobi_par_skel_simple");

	if (WORLD_SIZE_X % threads > 0) {
		printf(
				"WORLD_SIZE_X [%i] must be a multiple of THREADS [%i], which is not given. => FATAL ERROR!\n",
				WORLD_SIZE_X, threads);
		exit(1);
	}

	// Initialize copy-world
	init_copy_world();

	// Assign thread ids
	for (i = 0; i < threads; i++) {
		thread_data_tasks[i].thread_id = i;
	}

	// Split the data
	for (i = 0; i < threads; i++) {
		thread_data_tasks[i].offset = (WORLD_SIZE_X / threads) * i;
		thread_data_tasks[i].partsize = WORLD_SIZE_X / threads;
		thread_data_tasks[i].iterations = iterations;
	}

	void * args[THREADS];
	for (i = 0; i < threads; i++) {
		args[i] = & (thread_data_tasks[i]);
	}

	tas_runnable_t runnable = & algo_jacobi_par_skel_simple_runnable;

	tas_dataparallel_t dp = { runnable, args, threads }; // runnable, arguments, number of arguments
	tas_dataparallel_init(&dp, threads);


	for (j = 0; j < iterations / 2; j++) {

		// Flip
		for (i = 0; i < threads; i++) {
			thread_data_tasks[i].ptr_from = &world;
			thread_data_tasks[i].ptr_to = &world_temp;
		}

		tas_dataparallel_execute(&dp);

		// Flip
		for (i = 0; i < threads; i++) {
			thread_data_tasks[i].ptr_from = &world_temp;
			thread_data_tasks[i].ptr_to = &world;
		}

		tas_dataparallel_execute(&dp);

		// printf("T%i Task completed iteration B-%i...\n", my_data->thread_id, i);
	}

	tas_dataparallel_finalize(&dp);

	duration = logEnd("promote_world algo_jacobi_par_skel_simple");

#if TAS_POSIX == 1
	printf("DATA algo_jacobi_par_skel_simple iterations %i threads %i duration %lu\n", iterations, threads, duration);
#else
	printf("DATA algo_jacobi_par_skel_simple iterations %i threads %i duration %i\n", iterations, threads, duration);
#endif

}

