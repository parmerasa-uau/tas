#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "tas_demo_7.h"
#include "data.h"
// #include "skel_mw/skel_manager_worker.h"
#include "tas.h"
#include "tas_log.h"

#include "../platform.h"

// #include "skel_mw/skel_manager_worker.h"
// #include "skel_mw/skel_manager_config.h"

// #define DEBUG_LAPLACE 0

// #define COMPARTMENTS_PER_AXIS 2

#define COMPARTMENT_SIZE_X (WORLD_SIZE_X/COMPARTMENTS_PER_AXIS)
#define COMPARTMENT_SIZE_Y (WORLD_SIZE_Y/COMPARTMENTS_PER_AXIS)
#define COMPARTMENT_SIZE_Z (WORLD_SIZE_Z/COMPARTMENTS_PER_AXIS)

#define COMPARTMENT_COUNTER (COMPARTMENTS_PER_AXIS * COMPARTMENTS_PER_AXIS * COMPARTMENTS_PER_AXIS)

struct thread_data_gausseidel {
	int thread_id;
	int compartment; // Compartment to calculate
	pthread_mutex_t task_mutex; // Mutex for conditional
	pthread_cond_t task_cv; // Conditional
};

#if COMPARTMENTS_PER_AXIS == 2
/** Array with the top-left-front-points of eight compartments */
int reference_compartments[COMPARTMENT_COUNTER][3] = { { 0, 0, 0 }, { 1, 0, 0 },
		{ 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 0 }, { 1, 0, 1 }, { 0, 1, 1 }, { 1, 1,
				1 } };
#endif

#if COMPARTMENTS_PER_AXIS == 4
int reference_compartments[64][3] = { { 0, 0, 0 }, // 0 | D=0, X=0
 { 0, 0, 1 }, // 1l | D=1, X=1
 { 0, 1, 0 }, // 2 | D=1, X=1
 { 1, 0, 0 }, // 3 | D=1, X=1
 { 0, 1, 1 }, // 4 | D=2, X=1
 { 1, 0, 1 }, // 5 | D=2, X=1
 { 1, 1, 0 }, // 6 | D=2, X=1
 { 1, 1, 1 }, // 7 | D=3, X=1
 { 0, 0, 2 }, // 8 | D=2, X=2
 { 0, 2, 0 }, // 9 | D=2, X=2
 { 2, 0, 0 }, // 10 | D=2, X=2
 { 0, 1, 2 }, // 11 | D=3, X=2
 { 0, 2, 1 }, // 12 | D=3, X=2
 { 1, 0, 2 }, // 13 | D=3, X=2
 { 1, 2, 0 }, // 14 | D=3, X=2
 { 2, 0, 1 }, // 15 | D=3, X=2
 { 2, 1, 0 }, // 16 | D=3, X=2
 { 0, 2, 2 }, // 17 | D=4, X=2
 { 1, 1, 2 }, // 18 | D=4, X=2
 { 1, 2, 1 }, // 19 | D=4, X=2
 { 2, 0, 2 }, // 20 | D=4, X=2
 { 2, 1, 1 }, // 21 | D=4, X=2
 { 2, 2, 0 }, // 22 | D=4, X=2
 { 1, 2, 2 }, // 23 | D=5, X=2
 { 2, 1, 2 }, // 24 | D=5, X=2
 { 2, 2, 1 }, // 25 | D=5, X=2
 { 2, 2, 2 }, // 26 | D=6, X=2
 { 0, 0, 3 }, // 27 | D=3, X=3
 { 0, 3, 0 }, // 28 | D=3, X=3
 { 3, 0, 0 }, // 29 | D=3, X=3
 { 0, 1, 3 }, // 30 | D=4, X=3
 { 0, 3, 1 }, // 31 | D=4, X=3
 { 1, 0, 3 }, // 32 | D=4, X=3
 { 1, 3, 0 }, // 33 | D=4, X=3
 { 3, 0, 1 }, // 34 | D=4, X=3
 { 3, 1, 0 }, // 35 | D=4, X=3
 { 0, 2, 3 }, // 36 | D=5, X=3
 { 0, 3, 2 }, // 37 | D=5, X=3
 { 1, 1, 3 }, // 38 | D=5, X=3
 { 1, 3, 1 }, // 39 | D=5, X=3
 { 2, 0, 3 }, // 40 | D=5, X=3
 { 2, 3, 0 }, // 41 | D=5, X=3
 { 3, 0, 2 }, // 42 | D=5, X=3
 { 3, 1, 1 }, // 43 | D=5, X=3
 { 3, 2, 0 }, // 44 | D=5, X=3
 { 0, 3, 3 }, // 45 | D=6, X=3
 { 1, 2, 3 }, // 46 | D=6, X=3
 { 1, 3, 2 }, // 47 | D=6, X=3
 { 2, 1, 3 }, // 48 | D=6, X=3
 { 2, 3, 1 }, // 49 | D=6, X=3
 { 3, 0, 3 }, // 50 | D=6, X=3
 { 3, 1, 2 }, // 51 | D=6, X=3
 { 3, 2, 1 }, // 52 | D=6, X=3
 { 3, 3, 0 }, // 53 | D=6, X=3
 { 1, 3, 3 }, // 54 | D=7, X=3
 { 2, 2, 3 }, // 55 | D=7, X=3
 { 2, 3, 2 }, // 56 | D=7, X=3
 { 3, 1, 3 }, // 57 | D=7, X=3
 { 3, 2, 2 }, // 58 | D=7, X=3
 { 3, 3, 1 }, // 59 | D=7, X=3
 { 2, 3, 3 }, // 60 | D=8, X=3
 { 3, 2, 3 }, // 61 | D=8, X=3
 { 3, 3, 2 }, // 62 | D=8, X=3
 { 3, 3, 3 }, // 63 | D=9, X=3
 };
#endif

int compartments[COMPARTMENT_COUNTER][3];

int compartment_distances[10][12] = { { 0, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1 }, // D=0
		{ 1, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // D=1
		{ 4, 5, 6, 8, 9, 10, -1, -1, -1, -1, -1, -1 }, // D=2
		{ 7, 11, 12, 13, 14, 15, 16, 27, 28, 29, -1, -1 }, // D=3
		{ 17, 18, 19, 20, 21, 22, 30, 31, 32, 33, 34, 35 }, // D=4
		{ 23, 24, 25, 36, 37, 38, 39, 40, 41, 42, 43, 44 }, // D=5
		{ 26, 45, 46, 47, 48, 49, 50, 51, 52, 53, -1, -1 }, // D=6
		{ 54, 55, 56, 57, 58, 59, -1, -1, -1, -1, -1, -1 }, // D=7
		{ 60, 61, 62, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // D=8
		{ 63, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // D=9
		};

/*
 {0}, // D=0
 {1, 2, 3}, // D=1
 {4, 5, 6, 8, 9, 10}, // D=2
 {7, 11, 12, 13, 14, 15, 16, 27, 28, 29}, // D=3
 {17, 18, 19, 20, 21, 22, 30, 31, 32, 33, 34, 35}, // D=4
 {23, 24, 25, 36, 37, 38, 39, 40, 41, 42, 43, 44}, // D=5
 {26, 45, 46, 47, 48, 49, 50, 51, 52, 53}, // D=6
 {54, 55, 56, 57, 58, 59}, // D=7
 {60, 61, 62}, // D=8
 {63}, // D=9
 */

/**
 *   Calculated the top-left-front-points of compartments
 *   @param compartments Target array, eight points with three dimensions
 *   @param size Size of an array, typically half the size of the total world
 *   @param offset_x X-coordinate of the starting point
 *   @param offset_y Y-coordinate of the starting point
 *   @param offset_z Z-coordinate of the starting point
 **/
void calculate_compartments() {
	int a;

	if (WORLD_SIZE_X % COMPARTMENTS_PER_AXIS > 0) {
		printf(
				"WORLD_SIZE_X [%i] must be a multiple of %i, which is not given. => FATAL ERROR!\n",
				WORLD_SIZE_X, COMPARTMENTS_PER_AXIS);
		exit(1);
	}

	if (WORLD_SIZE_Y % COMPARTMENTS_PER_AXIS > 0) {
		printf(
				"WORLD_SIZE_Y [%i] must be a multiple of %i, which is not given. => FATAL ERROR!\n",
				WORLD_SIZE_Y, COMPARTMENTS_PER_AXIS);
		exit(1);
	}

	if (WORLD_SIZE_Z % COMPARTMENTS_PER_AXIS > 0) {
		printf(
				"WORLD_SIZE_Z [%i] must be a multiple of %i, which is not given. => FATAL ERROR!\n",
				WORLD_SIZE_Z, COMPARTMENTS_PER_AXIS);
		exit(1);
	}

	for (a = 0; a < COMPARTMENT_COUNTER; a++) {
		compartments[a][0] = reference_compartments[a][0] * COMPARTMENT_SIZE_X;
		compartments[a][1] = reference_compartments[a][1] * COMPARTMENT_SIZE_Y;
		compartments[a][2] = reference_compartments[a][2] * COMPARTMENT_SIZE_Z;

		/* for(b = 0; b < 3; b++) {
		 int value = 0;
		 if(b == 0) value += offset_x;
		 else if(b == 1) value += offset_y;
		 else if(b == 2) value += offset_z;
		 if(reference_compartments[a][b] == 1) value += size;
		 compartments[a][b] = value;
		 } */
	}
}

//// Implementation without barrier

void * promote_compartment_wrapper(void * threadarg) {
	struct thread_data_gausseidel * my_data;
	int index;

	// Get argument structure
	my_data = (struct thread_data_gausseidel *) threadarg;

	// Sequential implementation with single compartment
	index = my_data->compartment;

#ifdef DEBUG_LAPLACE
	printf(" - Calculating Compartment %i\n", index);
#endif

	int a, b, c;
	for (a = compartments[index][0];
			a < compartments[index][0] + COMPARTMENT_SIZE_X; a++) {
		for (b = compartments[index][1];
				b < compartments[index][1] + COMPARTMENT_SIZE_Y; b++) {
			for (c = compartments[index][2];
					c < compartments[index][2] + COMPARTMENT_SIZE_Z; c++) {
				if (isGoal(a, b, c)) {
					// world[a][b][c] = WEIGHT_SINK;
				} else if (isObstacle(a, b, c)) {
					// world[a][b][c] = WEIGHT_OBSTACLE;
				} else {
					world[a][b][c] = getAverage(world, a, b, c);
				}
			}
		}
	}

	return NULL ;
}

void algo_gaussseidel_par_skel(int iterations, int threads) {
	int i;
	long duration;

	logStartA("promote_world algo_gaussseidel_par_skel %i", iterations);

	// Calculate the dimensions of the compartments
	calculate_compartments();

	tas_dataparallel_t tas;
	tas.runnable = (tas_runnable_t) promote_compartment_wrapper;

	if (COMPARTMENTS_PER_AXIS == 2) {
		// Variables for thread
		int max_compartments_per_iteration = 4;
		struct thread_data_gausseidel thread_data_tasks[max_compartments_per_iteration];

		// Assign thread ids
		for (i = 0; i < max_compartments_per_iteration; i++) {
			thread_data_tasks[i].compartment = -1;
		}

		void * args[max_compartments_per_iteration];
		for (i = 0; i < max_compartments_per_iteration; i++) {
			args[i] = &(thread_data_tasks[i]);
		}

		tas.args = args;

		tas_dataparallel_init(&tas, threads);

		// Prologue

		// Phase 0
		thread_data_tasks[0].compartment = 0;

		tas.nr_args = 1;
		tas_dataparallel_execute(&tas);

		// Phase 1
		thread_data_tasks[0].compartment = 1;
		thread_data_tasks[1].compartment = 2;
		thread_data_tasks[2].compartment = 3;

		tas.nr_args = 3;
		tas_dataparallel_execute(&tas);

		// Kernel
		for (i = 0; i < iterations - 1; i++) {
			// Phase 2 and 0
			thread_data_tasks[0].compartment = 0;
			thread_data_tasks[1].compartment = 4;
			thread_data_tasks[2].compartment = 5;
			thread_data_tasks[3].compartment = 6;

			tas.nr_args = 4;
			tas_dataparallel_execute(&tas);

			// Phase 3 and 1
			thread_data_tasks[0].compartment = 7;
			thread_data_tasks[1].compartment = 1;
			thread_data_tasks[2].compartment = 2;
			thread_data_tasks[3].compartment = 3;

			tas.nr_args = 4;
			tas_dataparallel_execute(&tas);
		}

		// Epilogue

		// Phase 2
		thread_data_tasks[0].compartment = 4;
		thread_data_tasks[1].compartment = 5;
		thread_data_tasks[2].compartment = 6;

		tas.nr_args = 3;
		tas_dataparallel_execute(&tas);

		// Phase 3
		thread_data_tasks[0].compartment = 7;

		tas.nr_args = 1;
		tas_dataparallel_execute(&tas);

		tas_dataparallel_finalize(&tas);

	} else if (COMPARTMENTS_PER_AXIS == 4) {
		// Variables for thread
		int max_compartments_per_iteration = 32;
		struct thread_data_gausseidel thread_data_tasks[max_compartments_per_iteration];

		// Assign thread ids
		for (i = 0; i < max_compartments_per_iteration; i++) {
			thread_data_tasks[i].compartment = -1;
		}

		void * args[max_compartments_per_iteration];
		for (i = 0; i < max_compartments_per_iteration; i++) {
			args[i] = &(thread_data_tasks[i]);
		}

		tas.args = args;

		tas_dataparallel_init(&tas, threads);

		// Prologue

		// Prologue with distance 0
		// [1] [0]
		thread_data_tasks[0].compartment = 0;
		tas.nr_args = 1;
		tas_dataparallel_execute(&tas);

		// Prologue with distance 1
		// [3] [1, 2, 3]
		thread_data_tasks[0].compartment = 1;
		thread_data_tasks[1].compartment = 2;
		thread_data_tasks[2].compartment = 3;
		tas.nr_args = 3;
		tas_dataparallel_execute(&tas);

		// Prologue with distance 0 2
		// [7] [0, 4, 5, 6, 8, 9, 10]
		thread_data_tasks[0].compartment = 0;
		thread_data_tasks[1].compartment = 4;
		thread_data_tasks[2].compartment = 5;
		thread_data_tasks[3].compartment = 6;
		thread_data_tasks[4].compartment = 8;
		thread_data_tasks[5].compartment = 9;
		thread_data_tasks[6].compartment = 10;
		tas.nr_args = 7;
		tas_dataparallel_execute(&tas);

		// Prologue with distance 1 3
		// [13] [1, 2, 3, 7, 11, 12, 13, 14, 15, 16, 27, 28, 29]
		thread_data_tasks[0].compartment = 1;
		thread_data_tasks[1].compartment = 2;
		thread_data_tasks[2].compartment = 3;
		thread_data_tasks[3].compartment = 7;
		thread_data_tasks[4].compartment = 11;
		thread_data_tasks[5].compartment = 12;
		thread_data_tasks[6].compartment = 13;
		thread_data_tasks[7].compartment = 14;
		thread_data_tasks[8].compartment = 15;
		thread_data_tasks[9].compartment = 16;
		thread_data_tasks[10].compartment = 27;
		thread_data_tasks[11].compartment = 28;
		thread_data_tasks[12].compartment = 29;
		tas.nr_args = 13;
		tas_dataparallel_execute(&tas);

		// Prologue with distance 0 2 4
		// [19] [0, 4, 5, 6, 8, 9, 10, 17, 18, 19, 20, 21, 22, 30, 31, 32, 33, 34, 35]
		thread_data_tasks[0].compartment = 0;
		thread_data_tasks[1].compartment = 4;
		thread_data_tasks[2].compartment = 5;
		thread_data_tasks[3].compartment = 6;
		thread_data_tasks[4].compartment = 8;
		thread_data_tasks[5].compartment = 9;
		thread_data_tasks[6].compartment = 10;
		thread_data_tasks[7].compartment = 17;
		thread_data_tasks[8].compartment = 18;
		thread_data_tasks[9].compartment = 19;
		thread_data_tasks[10].compartment = 20;
		thread_data_tasks[11].compartment = 21;
		thread_data_tasks[12].compartment = 22;
		thread_data_tasks[13].compartment = 30;
		thread_data_tasks[14].compartment = 31;
		thread_data_tasks[15].compartment = 32;
		thread_data_tasks[16].compartment = 33;
		thread_data_tasks[17].compartment = 34;
		thread_data_tasks[18].compartment = 35;
		tas.nr_args = 19;
		tas_dataparallel_execute(&tas);

		// Prologue with distance 1 3 5
		// [25] [1, 2, 3, 7, 11, 12, 13, 14, 15, 16, 27, 28, 29, 23, 24, 25, 36, 37, 38, 39, 40, 41, 42, 43, 44]
		thread_data_tasks[0].compartment = 1;
		thread_data_tasks[1].compartment = 2;
		thread_data_tasks[2].compartment = 3;
		thread_data_tasks[3].compartment = 7;
		thread_data_tasks[4].compartment = 11;
		thread_data_tasks[5].compartment = 12;
		thread_data_tasks[6].compartment = 13;
		thread_data_tasks[7].compartment = 14;
		thread_data_tasks[8].compartment = 15;
		thread_data_tasks[9].compartment = 16;
		thread_data_tasks[10].compartment = 27;
		thread_data_tasks[11].compartment = 28;
		thread_data_tasks[12].compartment = 29;
		thread_data_tasks[13].compartment = 23;
		thread_data_tasks[14].compartment = 24;
		thread_data_tasks[15].compartment = 25;
		thread_data_tasks[16].compartment = 36;
		thread_data_tasks[17].compartment = 37;
		thread_data_tasks[18].compartment = 38;
		thread_data_tasks[19].compartment = 39;
		thread_data_tasks[20].compartment = 40;
		thread_data_tasks[21].compartment = 41;
		thread_data_tasks[22].compartment = 42;
		thread_data_tasks[23].compartment = 43;
		thread_data_tasks[24].compartment = 44;
		tas.nr_args = 25;
		tas_dataparallel_execute(&tas);

		// Prologue with distance 0 2 4 6
		// [29] [0, 4, 5, 6, 8, 9, 10, 17, 18, 19, 20, 21, 22, 30, 31, 32, 33, 34, 35, 26, 45, 46, 47, 48, 49, 50, 51, 52, 53]
		thread_data_tasks[0].compartment = 0;
		thread_data_tasks[1].compartment = 4;
		thread_data_tasks[2].compartment = 5;
		thread_data_tasks[3].compartment = 6;
		thread_data_tasks[4].compartment = 8;
		thread_data_tasks[5].compartment = 9;
		thread_data_tasks[6].compartment = 10;
		thread_data_tasks[7].compartment = 17;
		thread_data_tasks[8].compartment = 18;
		thread_data_tasks[9].compartment = 19;
		thread_data_tasks[10].compartment = 20;
		thread_data_tasks[11].compartment = 21;
		thread_data_tasks[12].compartment = 22;
		thread_data_tasks[13].compartment = 30;
		thread_data_tasks[14].compartment = 31;
		thread_data_tasks[15].compartment = 32;
		thread_data_tasks[16].compartment = 33;
		thread_data_tasks[17].compartment = 34;
		thread_data_tasks[18].compartment = 35;
		thread_data_tasks[19].compartment = 26;
		thread_data_tasks[20].compartment = 45;
		thread_data_tasks[21].compartment = 46;
		thread_data_tasks[22].compartment = 47;
		thread_data_tasks[23].compartment = 48;
		thread_data_tasks[24].compartment = 49;
		thread_data_tasks[25].compartment = 50;
		thread_data_tasks[26].compartment = 51;
		thread_data_tasks[27].compartment = 52;
		thread_data_tasks[28].compartment = 53;
		tas.nr_args = 29;
		tas_dataparallel_execute(&tas);

		// Prologue with distance 1 3 5 7
		// [31] [1, 2, 3, 7, 11, 12, 13, 14, 15, 16, 27, 28, 29, 23, 24, 25, 36, 37, 38, 39, 40, 41, 42, 43, 44, 54, 55, 56, 57, 58, 59]
		thread_data_tasks[0].compartment = 1;
		thread_data_tasks[1].compartment = 2;
		thread_data_tasks[2].compartment = 3;
		thread_data_tasks[3].compartment = 7;
		thread_data_tasks[4].compartment = 11;
		thread_data_tasks[5].compartment = 12;
		thread_data_tasks[6].compartment = 13;
		thread_data_tasks[7].compartment = 14;
		thread_data_tasks[8].compartment = 15;
		thread_data_tasks[9].compartment = 16;
		thread_data_tasks[10].compartment = 27;
		thread_data_tasks[11].compartment = 28;
		thread_data_tasks[12].compartment = 29;
		thread_data_tasks[13].compartment = 23;
		thread_data_tasks[14].compartment = 24;
		thread_data_tasks[15].compartment = 25;
		thread_data_tasks[16].compartment = 36;
		thread_data_tasks[17].compartment = 37;
		thread_data_tasks[18].compartment = 38;
		thread_data_tasks[19].compartment = 39;
		thread_data_tasks[20].compartment = 40;
		thread_data_tasks[21].compartment = 41;
		thread_data_tasks[22].compartment = 42;
		thread_data_tasks[23].compartment = 43;
		thread_data_tasks[24].compartment = 44;
		thread_data_tasks[25].compartment = 54;
		thread_data_tasks[26].compartment = 55;
		thread_data_tasks[27].compartment = 56;
		thread_data_tasks[28].compartment = 57;
		thread_data_tasks[29].compartment = 58;
		thread_data_tasks[30].compartment = 59;
		tas.nr_args = 31;
		tas_dataparallel_execute(&tas);

		// Kernel
		for (i = 0; i < iterations - 4; i++) {
			// Kernel with distance 0 2 4 6 8
			// [32] [0, 4, 5, 6, 8, 9, 10, 17, 18, 19, 20, 21, 22, 30, 31, 32, 33, 34, 35, 26, 45, 46, 47, 48, 49, 50, 51, 52, 53, 60, 61, 62]
			thread_data_tasks[0].compartment = 0;
			thread_data_tasks[1].compartment = 4;
			thread_data_tasks[2].compartment = 5;
			thread_data_tasks[3].compartment = 6;
			thread_data_tasks[4].compartment = 8;
			thread_data_tasks[5].compartment = 9;
			thread_data_tasks[6].compartment = 10;
			thread_data_tasks[7].compartment = 17;
			thread_data_tasks[8].compartment = 18;
			thread_data_tasks[9].compartment = 19;
			thread_data_tasks[10].compartment = 20;
			thread_data_tasks[11].compartment = 21;
			thread_data_tasks[12].compartment = 22;
			thread_data_tasks[13].compartment = 30;
			thread_data_tasks[14].compartment = 31;
			thread_data_tasks[15].compartment = 32;
			thread_data_tasks[16].compartment = 33;
			thread_data_tasks[17].compartment = 34;
			thread_data_tasks[18].compartment = 35;
			thread_data_tasks[19].compartment = 26;
			thread_data_tasks[20].compartment = 45;
			thread_data_tasks[21].compartment = 46;
			thread_data_tasks[22].compartment = 47;
			thread_data_tasks[23].compartment = 48;
			thread_data_tasks[24].compartment = 49;
			thread_data_tasks[25].compartment = 50;
			thread_data_tasks[26].compartment = 51;
			thread_data_tasks[27].compartment = 52;
			thread_data_tasks[28].compartment = 53;
			thread_data_tasks[29].compartment = 60;
			thread_data_tasks[30].compartment = 61;
			thread_data_tasks[31].compartment = 62;
			tas.nr_args = 32;
			tas_dataparallel_execute(&tas);

			// Kernel with distance 1 3 5 7 9
			// [32] [1, 2, 3, 7, 11, 12, 13, 14, 15, 16, 27, 28, 29, 23, 24, 25, 36, 37, 38, 39, 40, 41, 42, 43, 44, 54, 55, 56, 57, 58, 59, 63]
			thread_data_tasks[0].compartment = 1;
			thread_data_tasks[1].compartment = 2;
			thread_data_tasks[2].compartment = 3;
			thread_data_tasks[3].compartment = 7;
			thread_data_tasks[4].compartment = 11;
			thread_data_tasks[5].compartment = 12;
			thread_data_tasks[6].compartment = 13;
			thread_data_tasks[7].compartment = 14;
			thread_data_tasks[8].compartment = 15;
			thread_data_tasks[9].compartment = 16;
			thread_data_tasks[10].compartment = 27;
			thread_data_tasks[11].compartment = 28;
			thread_data_tasks[12].compartment = 29;
			thread_data_tasks[13].compartment = 23;
			thread_data_tasks[14].compartment = 24;
			thread_data_tasks[15].compartment = 25;
			thread_data_tasks[16].compartment = 36;
			thread_data_tasks[17].compartment = 37;
			thread_data_tasks[18].compartment = 38;
			thread_data_tasks[19].compartment = 39;
			thread_data_tasks[20].compartment = 40;
			thread_data_tasks[21].compartment = 41;
			thread_data_tasks[22].compartment = 42;
			thread_data_tasks[23].compartment = 43;
			thread_data_tasks[24].compartment = 44;
			thread_data_tasks[25].compartment = 54;
			thread_data_tasks[26].compartment = 55;
			thread_data_tasks[27].compartment = 56;
			thread_data_tasks[28].compartment = 57;
			thread_data_tasks[29].compartment = 58;
			thread_data_tasks[30].compartment = 59;
			thread_data_tasks[31].compartment = 63;
			tas.nr_args = 32;
			tas_dataparallel_execute(&tas);
		}

		// Epilogue

		// Epilog with distance 2 4 6 8
		// [31] [4, 5, 6, 8, 9, 10, 17, 18, 19, 20, 21, 22, 30, 31, 32, 33, 34, 35, 26, 45, 46, 47, 48, 49, 50, 51, 52, 53, 60, 61, 62]
		thread_data_tasks[0].compartment = 4;
		thread_data_tasks[1].compartment = 5;
		thread_data_tasks[2].compartment = 6;
		thread_data_tasks[3].compartment = 8;
		thread_data_tasks[4].compartment = 9;
		thread_data_tasks[5].compartment = 10;
		thread_data_tasks[6].compartment = 17;
		thread_data_tasks[7].compartment = 18;
		thread_data_tasks[8].compartment = 19;
		thread_data_tasks[9].compartment = 20;
		thread_data_tasks[10].compartment = 21;
		thread_data_tasks[11].compartment = 22;
		thread_data_tasks[12].compartment = 30;
		thread_data_tasks[13].compartment = 31;
		thread_data_tasks[14].compartment = 32;
		thread_data_tasks[15].compartment = 33;
		thread_data_tasks[16].compartment = 34;
		thread_data_tasks[17].compartment = 35;
		thread_data_tasks[18].compartment = 26;
		thread_data_tasks[19].compartment = 45;
		thread_data_tasks[20].compartment = 46;
		thread_data_tasks[21].compartment = 47;
		thread_data_tasks[22].compartment = 48;
		thread_data_tasks[23].compartment = 49;
		thread_data_tasks[24].compartment = 50;
		thread_data_tasks[25].compartment = 51;
		thread_data_tasks[26].compartment = 52;
		thread_data_tasks[27].compartment = 53;
		thread_data_tasks[28].compartment = 60;
		thread_data_tasks[29].compartment = 61;
		thread_data_tasks[30].compartment = 62;
		tas.nr_args = 31;
		tas_dataparallel_execute(&tas);

		// Epilog with distance 3 5 7 9
		// [29] [7, 11, 12, 13, 14, 15, 16, 27, 28, 29, 23, 24, 25, 36, 37, 38, 39, 40, 41, 42, 43, 44, 54, 55, 56, 57, 58, 59, 63]
		thread_data_tasks[0].compartment = 7;
		thread_data_tasks[1].compartment = 11;
		thread_data_tasks[2].compartment = 12;
		thread_data_tasks[3].compartment = 13;
		thread_data_tasks[4].compartment = 14;
		thread_data_tasks[5].compartment = 15;
		thread_data_tasks[6].compartment = 16;
		thread_data_tasks[7].compartment = 27;
		thread_data_tasks[8].compartment = 28;
		thread_data_tasks[9].compartment = 29;
		thread_data_tasks[10].compartment = 23;
		thread_data_tasks[11].compartment = 24;
		thread_data_tasks[12].compartment = 25;
		thread_data_tasks[13].compartment = 36;
		thread_data_tasks[14].compartment = 37;
		thread_data_tasks[15].compartment = 38;
		thread_data_tasks[16].compartment = 39;
		thread_data_tasks[17].compartment = 40;
		thread_data_tasks[18].compartment = 41;
		thread_data_tasks[19].compartment = 42;
		thread_data_tasks[20].compartment = 43;
		thread_data_tasks[21].compartment = 44;
		thread_data_tasks[22].compartment = 54;
		thread_data_tasks[23].compartment = 55;
		thread_data_tasks[24].compartment = 56;
		thread_data_tasks[25].compartment = 57;
		thread_data_tasks[26].compartment = 58;
		thread_data_tasks[27].compartment = 59;
		thread_data_tasks[28].compartment = 63;
		tas.nr_args = 29;
		tas_dataparallel_execute(&tas);

		// Epilog with distance 4 6 8
		// [25] [17, 18, 19, 20, 21, 22, 30, 31, 32, 33, 34, 35, 26, 45, 46, 47, 48, 49, 50, 51, 52, 53, 60, 61, 62]
		thread_data_tasks[0].compartment = 17;
		thread_data_tasks[1].compartment = 18;
		thread_data_tasks[2].compartment = 19;
		thread_data_tasks[3].compartment = 20;
		thread_data_tasks[4].compartment = 21;
		thread_data_tasks[5].compartment = 22;
		thread_data_tasks[6].compartment = 30;
		thread_data_tasks[7].compartment = 31;
		thread_data_tasks[8].compartment = 32;
		thread_data_tasks[9].compartment = 33;
		thread_data_tasks[10].compartment = 34;
		thread_data_tasks[11].compartment = 35;
		thread_data_tasks[12].compartment = 26;
		thread_data_tasks[13].compartment = 45;
		thread_data_tasks[14].compartment = 46;
		thread_data_tasks[15].compartment = 47;
		thread_data_tasks[16].compartment = 48;
		thread_data_tasks[17].compartment = 49;
		thread_data_tasks[18].compartment = 50;
		thread_data_tasks[19].compartment = 51;
		thread_data_tasks[20].compartment = 52;
		thread_data_tasks[21].compartment = 53;
		thread_data_tasks[22].compartment = 60;
		thread_data_tasks[23].compartment = 61;
		thread_data_tasks[24].compartment = 62;
		tas.nr_args = 25;
		tas_dataparallel_execute(&tas);

		// Epilog with distance 5 7 9
		// [19] [23, 24, 25, 36, 37, 38, 39, 40, 41, 42, 43, 44, 54, 55, 56, 57, 58, 59, 63]
		thread_data_tasks[0].compartment = 23;
		thread_data_tasks[1].compartment = 24;
		thread_data_tasks[2].compartment = 25;
		thread_data_tasks[3].compartment = 36;
		thread_data_tasks[4].compartment = 37;
		thread_data_tasks[5].compartment = 38;
		thread_data_tasks[6].compartment = 39;
		thread_data_tasks[7].compartment = 40;
		thread_data_tasks[8].compartment = 41;
		thread_data_tasks[9].compartment = 42;
		thread_data_tasks[10].compartment = 43;
		thread_data_tasks[11].compartment = 44;
		thread_data_tasks[12].compartment = 54;
		thread_data_tasks[13].compartment = 55;
		thread_data_tasks[14].compartment = 56;
		thread_data_tasks[15].compartment = 57;
		thread_data_tasks[16].compartment = 58;
		thread_data_tasks[17].compartment = 59;
		thread_data_tasks[18].compartment = 63;
		tas.nr_args = 19;
		tas_dataparallel_execute(&tas);

		// Epilog with distance 6 8
		// [13] [26, 45, 46, 47, 48, 49, 50, 51, 52, 53, 60, 61, 62]
		thread_data_tasks[0].compartment = 26;
		thread_data_tasks[1].compartment = 45;
		thread_data_tasks[2].compartment = 46;
		thread_data_tasks[3].compartment = 47;
		thread_data_tasks[4].compartment = 48;
		thread_data_tasks[5].compartment = 49;
		thread_data_tasks[6].compartment = 50;
		thread_data_tasks[7].compartment = 51;
		thread_data_tasks[8].compartment = 52;
		thread_data_tasks[9].compartment = 53;
		thread_data_tasks[10].compartment = 60;
		thread_data_tasks[11].compartment = 61;
		thread_data_tasks[12].compartment = 62;
		tas.nr_args = 13;
		tas_dataparallel_execute(&tas);

		// Epilog with distance 7 9
		// [7] [54, 55, 56, 57, 58, 59, 63]
		thread_data_tasks[0].compartment = 54;
		thread_data_tasks[1].compartment = 55;
		thread_data_tasks[2].compartment = 56;
		thread_data_tasks[3].compartment = 57;
		thread_data_tasks[4].compartment = 58;
		thread_data_tasks[5].compartment = 59;
		thread_data_tasks[6].compartment = 63;
		tas.nr_args = 7;
		tas_dataparallel_execute(&tas);

		// Epilog with distance 8
		// [3] [60, 61, 62]
		thread_data_tasks[0].compartment = 60;
		thread_data_tasks[1].compartment = 61;
		thread_data_tasks[2].compartment = 62;
		tas.nr_args = 3;
		tas_dataparallel_execute(&tas);

		// Epilog with distance 9
		// [1] [63]
		thread_data_tasks[0].compartment = 63;
		tas.nr_args = 1;
		tas_dataparallel_execute(&tas);

		tas_dataparallel_finalize(&tas);
	} else {
		printf("ERROR: %i compartments is not supported.\n",
				COMPARTMENTS_PER_AXIS);
	}

	duration = logEnd("promote_world algo_gaussseidel_par_skel");

#if TAS_POSIX == 1
	printf("DATA algo_gaussseidel_par_skel_%i iterations %i threads %i duration %lu\n", COMPARTMENTS_PER_AXIS, iterations, threads, duration);
#else
	printf("DATA algo_gaussseidel_par_skel_%i iterations %i threads %i duration %i\n", COMPARTMENTS_PER_AXIS, iterations, threads, duration);
#endif
}

void algo_gaussseidel_par(int iterations, int threads) {
	int i;

	logStartA("promote_world algo_gauss-seidel_par %i", iterations);

	// Calculate the dimensions of the compartments
	calculate_compartments();

	// Variables for thread
	int used_threads = threads;
	pthread_t thread_tasks[used_threads];
	struct thread_data_gausseidel thread_data_tasks[used_threads];

	// Assign thread ids
	for (i = 0; i < used_threads; i++) {
		thread_data_tasks[i].thread_id = i;
		thread_data_tasks[i].compartment = -1;
	}

	// Prologue

	// Phase 0
	thread_data_tasks[0].compartment = 0;

	pthread_create(&thread_tasks[0], NULL, promote_compartment_wrapper,
			(void *) &thread_data_tasks[0]);

	pthread_join(thread_tasks[0], NULL );

	// Phase 1
	thread_data_tasks[1].compartment = 1;
	thread_data_tasks[2].compartment = 2;
	thread_data_tasks[3].compartment = 3;

	pthread_create(&thread_tasks[1], NULL, promote_compartment_wrapper,
			(void *) &thread_data_tasks[1]);
	pthread_create(&thread_tasks[2], NULL, promote_compartment_wrapper,
			(void *) &thread_data_tasks[2]);
	pthread_create(&thread_tasks[3], NULL, promote_compartment_wrapper,
			(void *) &thread_data_tasks[3]);

	pthread_join(thread_tasks[1], NULL );
	pthread_join(thread_tasks[2], NULL );
	pthread_join(thread_tasks[3], NULL );

	// Kernel
	for (i = 0; i < iterations - 1; i++) {
		// Phase 2 and 0
		thread_data_tasks[0].compartment = 0;
		thread_data_tasks[1].compartment = 4;
		thread_data_tasks[2].compartment = 5;
		thread_data_tasks[3].compartment = 6;

		pthread_create(&thread_tasks[0], NULL, promote_compartment_wrapper,
				(void *) &thread_data_tasks[0]);
		pthread_create(&thread_tasks[1], NULL, promote_compartment_wrapper,
				(void *) &thread_data_tasks[1]);
		pthread_create(&thread_tasks[2], NULL, promote_compartment_wrapper,
				(void *) &thread_data_tasks[2]);
		pthread_create(&thread_tasks[3], NULL, promote_compartment_wrapper,
				(void *) &thread_data_tasks[3]);

		pthread_join(thread_tasks[0], NULL );
		pthread_join(thread_tasks[1], NULL );
		pthread_join(thread_tasks[2], NULL );
		pthread_join(thread_tasks[3], NULL );

		// Phase 3 and 1
		thread_data_tasks[0].compartment = 7;
		thread_data_tasks[1].compartment = 1;
		thread_data_tasks[2].compartment = 2;
		thread_data_tasks[3].compartment = 3;

		pthread_create(&thread_tasks[0], NULL, promote_compartment_wrapper,
				(void *) &thread_data_tasks[0]);
		pthread_create(&thread_tasks[1], NULL, promote_compartment_wrapper,
				(void *) &thread_data_tasks[1]);
		pthread_create(&thread_tasks[2], NULL, promote_compartment_wrapper,
				(void *) &thread_data_tasks[2]);
		pthread_create(&thread_tasks[3], NULL, promote_compartment_wrapper,
				(void *) &thread_data_tasks[3]);

		pthread_join(thread_tasks[0], NULL );
		pthread_join(thread_tasks[1], NULL );
		pthread_join(thread_tasks[2], NULL );
		pthread_join(thread_tasks[3], NULL );
	}

	// Epilogue

	// Phase 2
	thread_data_tasks[1].compartment = 4;
	thread_data_tasks[2].compartment = 5;
	thread_data_tasks[3].compartment = 6;

	pthread_create(&thread_tasks[1], NULL, promote_compartment_wrapper,
			(void *) &thread_data_tasks[1]);
	pthread_create(&thread_tasks[2], NULL, promote_compartment_wrapper,
			(void *) &thread_data_tasks[2]);
	pthread_create(&thread_tasks[3], NULL, promote_compartment_wrapper,
			(void *) &thread_data_tasks[3]);

	pthread_join(thread_tasks[1], NULL );
	pthread_join(thread_tasks[2], NULL );
	pthread_join(thread_tasks[3], NULL );

	// Phase 3
	thread_data_tasks[0].compartment = 7;

	pthread_create(&thread_tasks[0], NULL, promote_compartment_wrapper,
			(void *) &thread_data_tasks[0]);

	pthread_join(thread_tasks[0], NULL );

	logEnd("promote_world algo_gauss-seidel_par");
}

//// Implementation with barrier

/* void promote_compartment_wrapper_barrier(int thread_id, int index) {
	// Sequential implementation with single compartment
	int a, b, c;

	if (DEBUG)
		printf("---------------- [T%i] Processing %i starts...\n", thread_id,
				index);

	for (a = compartments[index][0];
			a < compartments[index][0] + COMPARTMENT_SIZE_X; a++) {
		for (b = compartments[index][1];
				b < compartments[index][1] + COMPARTMENT_SIZE_Y; b++) {
			for (c = compartments[index][2];
					c < compartments[index][2] + COMPARTMENT_SIZE_Z; c++) {
				if (isGoal(a, b, c)) {
					// world[a][b][c] = WEIGHT_SINK;
				} else if (isObstacle(a, b, c)) {
					// world[a][b][c] = WEIGHT_OBSTACLE;
				} else {
					world[a][b][c] = getAverage(world, a, b, c);
				}
			}
		}
	}

	if (DEBUG)
		printf("---------------- [T%i] Processing %i finished.\n", thread_id,
				index);

	return;
}

void add_with_distance(int distance, manager_worker_t * mw_data) {
	int i;

	for (i = 0; i < 12; i++) {
		if (compartment_distances[distance][i] >= 0)
			manager_worker_enqueue(mw_data, compartment_distances[distance][i]);
	}
}

void algo_gaussseidel_par_barrier_mw(int iterations) {
	int i, j, rc;

	logStartA("promote_world algo_gauss-seidel_par_barrier_%i",
			COMPARTMENTS_PER_AXIS);

	// Calculate the dimensions of the compartments
	calculate_compartments();

	manager_worker_t mwdata;

	manager_worker_init(&mwdata, promote_compartment_wrapper_barrier);

	if (COMPARTMENTS_PER_AXIS == 4) {

		add_with_distance(0, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(1, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(0, &mwdata);
		add_with_distance(2, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(1, &mwdata);
		add_with_distance(3, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(0, &mwdata);
		add_with_distance(2, &mwdata);
		add_with_distance(4, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(1, &mwdata);
		add_with_distance(3, &mwdata);
		add_with_distance(5, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(0, &mwdata);
		add_with_distance(2, &mwdata);
		add_with_distance(4, &mwdata);
		add_with_distance(6, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(1, &mwdata);
		add_with_distance(3, &mwdata);
		add_with_distance(5, &mwdata);
		add_with_distance(7, &mwdata);
		manager_worker_complete(&mwdata);

		// Kernel
		for (i = 0; i < iterations - 4; i++) {
			add_with_distance(0, &mwdata);
			add_with_distance(2, &mwdata);
			add_with_distance(4, &mwdata);
			add_with_distance(6, &mwdata);
			add_with_distance(8, &mwdata);
			manager_worker_complete(&mwdata);

			add_with_distance(1, &mwdata);
			add_with_distance(3, &mwdata);
			add_with_distance(5, &mwdata);
			add_with_distance(7, &mwdata);
			add_with_distance(9, &mwdata);
			manager_worker_complete(&mwdata);
		}

		add_with_distance(2, &mwdata);
		add_with_distance(4, &mwdata);
		add_with_distance(6, &mwdata);
		add_with_distance(8, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(3, &mwdata);
		add_with_distance(5, &mwdata);
		add_with_distance(7, &mwdata);
		add_with_distance(9, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(4, &mwdata);
		add_with_distance(6, &mwdata);
		add_with_distance(8, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(5, &mwdata);
		add_with_distance(7, &mwdata);
		add_with_distance(9, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(6, &mwdata);
		add_with_distance(8, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(7, &mwdata);
		add_with_distance(9, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(8, &mwdata);
		manager_worker_complete(&mwdata);

		add_with_distance(9, &mwdata);
		manager_worker_complete(&mwdata);

	} else if (COMPARTMENTS_PER_AXIS == 2) {

		// Phase 0
		manager_worker_enqueue(&mwdata, 0);
		manager_worker_complete(&mwdata);

		// Phase 1
		manager_worker_enqueue(&mwdata, 1);
		manager_worker_enqueue(&mwdata, 2);
		manager_worker_enqueue(&mwdata, 3);

		manager_worker_complete(&mwdata);

		// Kernel
		for (i = 0; i < iterations - 1; i++) {
			// Phase 2 and 0
			manager_worker_enqueue(&mwdata, 4);
			manager_worker_enqueue(&mwdata, 5);
			manager_worker_enqueue(&mwdata, 6);

			manager_worker_enqueue(&mwdata, 0);

			manager_worker_complete(&mwdata);

			// Phase 3 and 1
			manager_worker_enqueue(&mwdata, 7);

			manager_worker_enqueue(&mwdata, 1);
			manager_worker_enqueue(&mwdata, 2);
			manager_worker_enqueue(&mwdata, 3);

			manager_worker_complete(&mwdata);
		}

		// Epilogue

		// Phase 2
		manager_worker_enqueue(&mwdata, 4);
		manager_worker_enqueue(&mwdata, 5);
		manager_worker_enqueue(&mwdata, 6);

		manager_worker_complete(&mwdata);

		// Phase 3
		manager_worker_enqueue(&mwdata, 7);

		manager_worker_complete(&mwdata);

	} else {
		printf("COMPARTMENTS_PER_AXIS must be 2 or 4, not %i. => ERROR, EXIT\n",
				COMPARTMENTS_PER_AXIS);
		exit(1);
	}

	manager_worker_finish(&mwdata);

	logEndA("promote_world algo_gauss-seidel_par_barrier_%i",
			COMPARTMENTS_PER_AXIS);
} */
