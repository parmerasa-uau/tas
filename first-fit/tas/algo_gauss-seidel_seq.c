#include "data.h"
#include "main.h"

void algo_gaussseidel_seq(int iterations) {
	int a, b, c, i;

	logStart("promote_world algo_gauss-seidel_seq");

	// Gauss-Seidel
	for (i = 0; i < iterations; i++) {
		for (a = 0; a < WORLD_SIZE_X; a++) {
			for (b = 0; b < WORLD_SIZE_Y; b++) {
				for (c = 0; c < WORLD_SIZE_Z; c++) {
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

		/* if (world[getPosition()[0]][getPosition()[1]][getPosition()[2]] == 0) {
		 printf("For sure not enough iterations after %i iterations\n", i);
		 } */
	}

	logEnd("promote_world algo_gauss-seidel_seq");
}
