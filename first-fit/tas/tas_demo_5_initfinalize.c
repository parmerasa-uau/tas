#include "tas.h"

#define CEILING_POS(X) ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))

 #define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

 #define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

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
#include "tas_demo_5.h"
#include "../platform.h"
#include <math.h>
#include <float.h>
#include <limits.h>
#include "tas.h"
#include "tas_log.h"

#define GA_TEST_MCCORMICK 1
#define GA_TEST_ROSENBROCK 2
#define GA_TEST_COLLATZ 3

#define GA_TEST_FUNCTION GA_TEST_ROSENBROCK

#define GA_POP_SIZE 200
#define GA_GENERATIONS 500
#define GA_CROSS 0.6
#define GA_RAND 0.6
#define GA_SURVIVORS 0.125

#define GA_WORST_RESULT FLT_MAX

#if GA_TEST_FUNCTION == GA_TEST_MCCORMICK
#define MIN_X -1.5
#define MAX_X 5
#define MIN_Y -3
#define MAX_Y 4
#endif

#if GA_TEST_FUNCTION == GA_TEST_ROSENBROCK
#define MIN_X -100
#define MAX_X 100
#define MIN_Y -100
#define MAX_Y 100
#endif

#if GA_TEST_FUNCTION == GA_TEST_COLLATZ
#define MIN_X 1
#define MAX_X 1000
#define MIN_Y 1
#define MAX_Y 1000
#endif

#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile ticketlock_t lock_random;
#else
pthread_mutex_t lock_random = PTHREAD_MUTEX_INITIALIZER;
#endif

#define N 25

// Random number between 0 and 1
// http://de.wikipedia.org/wiki/Mersenne-Twister -- TT800
double rando() {
	const int M = 7;
	const unsigned int A[2] = { 0, 0x8ebfd028 };

	ticket_lock(&lock_random); // ID=rando_lock
	static unsigned int y[N];
	static int index = N + 1;

	if (index >= N) {
		int k;
		if (index > N) {
			unsigned r = 9, s = 3402;
			for (k = 0; k < N; ++k) {
				r = 509845221 * r + 3;
				s *= s + 1;
				y[k] = s + (r >> 10);
			}
		}
		for (k = 0; k < N - M; ++k)
			y[k] = y[k + M] ^ (y[k] >> 1) ^ A[y[k] & 1];
		for (; k < N; ++k)
			y[k] = y[k + (M - N)] ^ (y[k] >> 1) ^ A[y[k] & 1];
		index = 0;
	}

	unsigned e = y[index++];
	ticket_unlock(&lock_random); // ID=rando_lock

	e ^= (e << 7) & 0x2b5b2500;
	e ^= (e << 15) & 0xdb8b0000;
	e ^= (e >> 16);

	return (double) e / UINT_MAX;
}

// #define rando() ((double) rand() / (RAND_MAX))

typedef struct point {
	float x, y, result;
} point_t;

#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile point_t generation[GA_POP_SIZE];
#else
point_t generation[GA_POP_SIZE];
#endif

float evaluate(float x, float y) {
#if GA_TEST_FUNCTION == GA_TEST_MCCORMICK
	return sin((x + y)) + ((x - y) * (x - y)) - (1.5 * x) + (2.5 * y) + 1; // McCormick function
#endif

#if GA_TEST_FUNCTION == GA_TEST_ROSENBROCK
	return ((1 - x) * (1 - x)) + 100 * (y - (x * x)) * (y - (x * x)); // http://en.wikipedia.org/wiki/Rosenbrock_function
#endif

#if GA_TEST_FUNCTION == GA_TEST_COLLATZ
	int n = x * 1000 + y;
	int counter = 0;

	if( n == 0) return GA_WORST_RESULT;

	while(n > 1) {
		counter++;
		if(n % 2 == 0) { // even
			n = n / 2;
		} else { // odd
			n = 3 * n + 1;
		}
	}
	return ((double)1 / (double)counter);
#endif
}

int compare(const void * pa, const void * pb) {
	const point_t * a = pa;
	const point_t * b = pb;

	// printf("Now comparing %f %f\n", a->result, b->result);

	if (a->result > b->result)
		return 1;
	else if (a->result < b->result)
		return -1;

	if (a->y > b->y)
		return 1;
	else if (a->y < b->y)
		return -1;

	if (a->x > b->x)
		return 1;
	else if (a->x < b->x)
		return -1;
	else
		return 0;
}

int equals(point_t a, point_t b) {
	return a.result == b.result && a.x == b.x && a.y == b.y;
}

float get_random(float min, float max) {
	return min + (max - min) * rando();
}

float get_new_value(float parent_a, float parent_b, float min, float max) {
	if (rando() <= GA_CROSS) {
		if (rando() <= 0.5) {
			return parent_a;
		} else {
			return parent_b;
		}
	} else if (rando() <= GA_RAND) {
		return get_random(min, max);
	} else {
		return parent_a;
	}
}

typedef struct ga_args {
	int first, last;
	char all_random;
	int survivors;
} ga_args_t;

void * init_and_evaluate(void * args) {

	struct ga_args * my_data;
	my_data = (struct ga_args *) args;

	int i;
	int survivors = my_data->survivors;

	for (i = my_data->first; i <= my_data->last; i++) {

		// Initialize
		if (my_data->all_random == 0) {
			int parent_a = survivors * rando();
			int parent_b = survivors * rando();
			generation[i].x = get_new_value(generation[parent_a].x,
					generation[parent_b].x, MIN_X, MAX_X);
			generation[i].y = get_new_value(generation[parent_a].y,
					generation[parent_b].y, MIN_Y, MAX_Y);
		} else {
			generation[i].x = get_random(MIN_X, MAX_X);
			generation[i].y = get_random(MIN_Y, MAX_Y);
		}

		// Evaluate
		generation[i].result = evaluate(generation[i].x, generation[i].y);

		// printf("Individual (%i) %i: %f <= %f %f\n", my_data->all_random, i, generation[i].result, generation[i].x, generation[i].y);
	}

	return NULL;
}

// Data structures for data parallel pattern
#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile ga_args_t my_ga_args_data[TOTAL_PROC_NUM];
SHARED_VARIABLE(membase_uncached0) volatile void * my_ga_args[TOTAL_PROC_NUM];
SHARED_VARIABLE(membase_uncached0) volatile tas_dataparallel_t dp_ga = {(tas_runnable_t) init_and_evaluate, my_ga_args, TOTAL_PROC_NUM};
#else
ga_args_t my_ga_args_data[TOTAL_PROC_NUM];
void * my_ga_args[TOTAL_PROC_NUM];
tas_dataparallel_t dp_ga = { (tas_runnable_t) init_and_evaluate, my_ga_args,
		TOTAL_PROC_NUM };
#endif

void demo_5_main_core_0() {
	int i, iteration;

#if GA_TEST_FUNCTION == GA_TEST_MCCORMICK
	PFL
	printf("GENETIC ALGORITHM => MCCORMICK\n");
	PFU
#endif

#if GA_TEST_FUNCTION == GA_TEST_ROSENBROCK
	PFL
	printf("GENETIC ALGORITHM => ROSENBROCK\n");
	PFU
#endif

	logStart("Genetic algorithm");

#if TAS_POSIX == 0
	ticket_init(&lock_random);
#endif

	// Task Parallel Pattern
	for (i = 0; i < TOTAL_PROC_NUM; i++) {
		my_ga_args[i] = &(my_ga_args_data[i]);
	}

	for (iteration = 0; iteration < GA_GENERATIONS; iteration++) {
		// Initialize and evaluate
		if (iteration == 0 || iteration == 1) {
			int survivors = 0;
			if (iteration == 0) {
				// generation[0].x = -0.54719; // McCormick Opt
				// generation[0].y = -1.54719; // McCormick Opt
				// generation[0].x = 1; // Rosenbrock Opt
				// generation[0].y = 1; // Rosenbrock Opt
			} else if (iteration > 0) { // and beyond....
				survivors = GA_POP_SIZE * GA_SURVIVORS;
			}

			int individuals_per_worker =
					CEILING_POS((double)(GA_POP_SIZE - survivors) / (double)TOTAL_PROC_NUM);

			for (i = 0; i < TOTAL_PROC_NUM; i++) {
				if(iteration == 0) my_ga_args_data[i].all_random = 1;
				else my_ga_args_data[i].all_random = 0;
				my_ga_args_data[i].survivors = survivors;
				my_ga_args_data[i].first = survivors + i * individuals_per_worker;
				my_ga_args_data[i].last = MIN(survivors + (i + 1) * individuals_per_worker, GA_POP_SIZE) - 1;
				printf("Iteration %i -- Worker %i -- %i survivors - from %i to %i\n",
				 		iteration, i, my_ga_args_data[i].survivors, my_ga_args_data[i].first, my_ga_args_data[i].last);
			}
		}

		/* for(i = 0; i < my_ga_args_data[0].survivors; i++) {
			printf("SURVIVOR  (%i) %i: %f <= %f %f\n", 0, i, generation[i].result, generation[i].x, generation[i].y);
		} */
		
		// Init Skeleton
		tas_dataparallel_init(&dp_ga, TOTAL_PROC_NUM); // ID=genetic_dp_init

		// logStart("Init and evaluate");
		tas_dataparallel_execute(&dp_ga); // ID=genetic_dp_execute
		// logEnd("Init and evaluate");
		
		// Finalize skeleton
		tas_dataparallel_finalize(&dp_ga); // ID=genetic_dp_finalize

		
		// Sort
		// logStart("Sort A");
		qsort(generation, GA_POP_SIZE, sizeof(point_t), compare);
		// logEnd("Sort A");
		
		// Remove duplicates
		// logStart("Remove Duplicates");
		int unique_count = 1;
		int last_unique = 0;
		for (i = 1; i < GA_POP_SIZE; i++) {
			if (equals(generation[last_unique], generation[i])) {
				generation[i].result = GA_WORST_RESULT;
			} else {
				last_unique = i;
				unique_count++;
			}
		}
		// logEnd("Remove Duplicates");

		// Sort
		// logStart("Sort B");
		qsort(generation, GA_POP_SIZE, sizeof(point_t), compare);
		// logEnd("Sort B");

		// Find best
		if ((iteration + 1) % 10 == 0) {
			int best_index = 0;
			float best_value = generation[0].result;
			for (i = 0; i < GA_POP_SIZE; i++) {
				if (generation[i].result < best_value) {
					best_value = generation[i].result;
					best_index = i;

					printf("Best values for x = %f, y = %f => %f\n",
							generation[i].x, generation[i].y,
							(double) 1 / generation[i].result);
				}
				// printf("[%i] DATA %f %f %f\n", iteration, generation[i].x, generation[i].y, generation[i].result);
				break;
			}

			printf("[%i] Best values for x = %f, y = %f => %f\n", iteration,
					generation[best_index].x, generation[best_index].y,
					(double) 1 / generation[best_index].result);
		}
	}

	logEnd("Genetic algorithm");

}
