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
#include "../platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "tas_stopwatch.h"
#include "tas_log.h"

// #include "heightmap.h" // Contains heightmap

#include "data.h"
#include "tas_demo_7.h"

double world[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z];
double world_temp[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z];
char global_obstacles[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z];

int position[3];
int goal[3];

void findWaypoints() {
	int position[3]; // Current position
	int i;
	double path_length = 0; // Total length of the path
	int path_steps = 0; // Total length of the path in steps

	logStart("findWaypoints");

	position[0] = getPosition()[0];
	position[1] = getPosition()[1];
	position[2] = getPosition()[2];

	for (i = 0; i < 1024; i++) {
		int a, b, c;

		double best_value = world[position[0]][position[1]][position[2]]; // So far best weigt == current weight
		int next[3] = { -1, -1, -1 }; // Next position

		for (a = -1; a <= 1; a++) {
			if (position[0] + a >= 0 && position[0] + a < WORLD_SIZE_X) {
				for (b = -1; b <= 1; b++) {
					if (position[1] + b >= 0 && position[1] + b < WORLD_SIZE_Y) {
						for (c = -1; c <= 1; c++) {
							if (position[2] + c >= 0 && position[2] + c < WORLD_SIZE_Z) {
								if (!isObstacle(position[0] + a, position[1] + b, position[2] + c)) {
									double new_value = world[position[0] + a][position[1] + b][position[2] + c];
									/* if (LL_DEBUG || 1)
									 printf(
									 "Surrounding point [%i, %i, %i] has value %2.12f (%e) - best know has value %2.12f (%e)\n",
									 position[0] + a, position[1]
									 + b, position[2] + c,
									 new_value, best_value); */
									if (new_value > best_value) {
										best_value = new_value;
										next[0] = position[0] + a;
										next[1] = position[1] + b;
										next[2] = position[2] + c;
									}
								}
							}
						}
					}
				}
			}
		}

		printf("Next waypoint: [%i, %i, %i] with value %2.12f (%e)\n", next[0], next[1], next[2], best_value,
				best_value);

		// Calculate path length
		int del_0 = position[0] - next[0];
		int del_1 = position[1] - next[1];
		int del_2 = position[2] - next[2];

		del_0 *= del_0;
		del_1 *= del_1;
		del_2 *= del_2;

		if (del_0 + del_1 + del_2 == 1)
			path_length += 1;
		if (del_0 + del_1 + del_2 == 2)
			path_length += 1.414; // SQRT(2)
		if (del_0 + del_1 + del_2 == 3)
			path_length += 1.732; // SQRT(3)

		path_steps++;

		if (isGoal(next[0], next[1], next[2]))
			break;
		else {
			// Set found waypoint as next position
			position[0] = next[0];
			position[1] = next[1];
			position[2] = next[2];
		}

	}

	printf("Total path length: %f\n", path_length);
	printf("Total path length in steps: %i\n", path_steps);

	logEnd("findWaypoints");
}

void demo_7_main_core_0() {

	int threads = TOTAL_PROC_NUM;

	// Initialize obstacle map from heightmap
	if (1) {
		logStart("initialize obstacle map");

		// Recode heightmap to global_obstacles
		int a, b, c;

		for (a = 0; a < WORLD_SIZE_X; a++) {
			for (b = 0; b < WORLD_SIZE_Y; b++) {
				int height = getHeightmapValue(a, b);

				// some regions cannot be flown over
				if (height > 174)
					height = 255;

				for (c = 0; c < height; c++) {
					global_obstacles[a][b][c] = 1;
				}
				for (c = height; c < WORLD_SIZE_Z; c++) {
					global_obstacles[a][b][c] = 0;
				}
			}
		}

		logEnd("initialize obstacle map");
	}

	// Set goal
	setGoal(WORLD_SIZE_X - 3, WORLD_SIZE_Y - 3, getHeightmapValue(WORLD_SIZE_X - 3, WORLD_SIZE_Y - 3) + 2);

	// Set current position
	setPosition(3, 3, getHeightmapValue(3, 3) + 2);

	// Init weightmap from obstacle map
	if (1) {
		int a, b, c;

		logStart("initialize weightmap from obstacle map");

		for (a = 0; a < WORLD_SIZE_X; a++) {
			for (b = 0; b < WORLD_SIZE_Y; b++) {
				for (c = 0; c < WORLD_SIZE_Z; c++) {
					if (isGoal(a, b, c)) {
						world[a][b][c] = WEIGHT_SINK;
					} else if (isObstacle(a, b, c)) {
						world[a][b][c] = WEIGHT_OBSTACLE;
					} else {
						world[a][b][c] = WEIGHT_INIT;
					}
				}
			}
		}

		logEnd("initialize weightmap from obstacle map");
	}

	// Propagate weights with selected algorithm
	promote_world(ITERATIONS, threads);

	// Find waypoints to goal
	findWaypoints();

	printf("Iterations: %i\n", ITERATIONS);

	return;
}

