#include "data.h"
#include "main.h"
#include "log.h"

double world_temp[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z];

void algo_jacobi_seq(int iterations) {
	int a, b, c, i;

	logStart("promote_world algo_jacobi_seq");

	// Initialize copy-world
	if (1) {
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

	for (i = 0; i < iterations / 2; i++) {
		for (a = 0; a < WORLD_SIZE_X; a++) {
			for (b = 0; b < WORLD_SIZE_Y; b++) {
				for (c = 0; c < WORLD_SIZE_Z; c++) {
					if (isGoal(a, b, c)) {
						// world_temp[a][b][c] = WEIGHT_SINK;
						// world[a][b][c] = WEIGHT_SINK;
					} else if (isObstacle(a, b, c)) {
						// world_temp[a][b][c] = WEIGHT_OBSTACLE;
						// world[a][b][c] = WEIGHT_OBSTACLE;
					} else {
						world_temp[a][b][c] = getAverage(world, a, b, c);
					}
				}
			}
		}

		for (a = 0; a < WORLD_SIZE_X; a++) {
			for (b = 0; b < WORLD_SIZE_Y; b++) {
				for (c = 0; c < WORLD_SIZE_Z; c++) {
					if (isGoal(a, b, c)) {
						// world_temp[a][b][c] = WEIGHT_SINK;
						// world[a][b][c] = WEIGHT_SINK;
					} else if (isObstacle(a, b, c)) {
						// world_temp[a][b][c] = WEIGHT_OBSTACLE;
						// world[a][b][c] = WEIGHT_OBSTACLE;
					} else {
						world[a][b][c] = getAverage(world_temp, a, b, c);
					}
				}
			}
		}

		/* if (world[getPosition()[0]][getPosition()[1]][getPosition()[2]] == 0) {
		 printf("Not enough iterations after %i iterations\n", (2 * i));
		 } */
	}

	logEnd("promote_world algo_jacobi_seq");
}
