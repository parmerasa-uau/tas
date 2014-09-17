#include <stdio.h>

#include "data.h"
#include "heightmap.h" // Contains heightmap

#include "tas_demo_7.h"

/* extern double world[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z];
extern char global_obstacles[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z];

extern int position[3];
extern int goal[3]; */

int * getPosition() {
	return position;
}

void setPosition(int a, int b, int c) {
	position[0] = a;
	position[1] = b;
	position[2] = c;
}

int * getGoal() {
	return goal;
}

void setGoal(int a, int b, int c) {
	goal[0] = a;
	goal[1] = b;
	goal[2] = c;
}

char isGoal(int a, int b, int c) {
	return (goal[0] == a) && (goal[1] == b) && (goal[2] == c);
}

unsigned char getHeightmapValue(int x, int y) {
	int value = heightmap[x][y];
	return value;
}

char isObstacle(int x, int y, int z) {
	char result = global_obstacles[x][y][z];
	return result;
}

double getAverage(double my_world[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z],
		int a, int b, int c) {
	double value = 0; // world[a][b][c];
	int counter = 6;

	if (a > 0 && a < WORLD_SIZE_X - 1) {
		value += my_world[a - 1][b][c];
		value += my_world[a + 1][b][c];
	} else {
		if (a > 0) {
			value += my_world[a - 1][b][c];
			// counter--;
		}
		if (a < WORLD_SIZE_X - 1) {
			value += my_world[a + 1][b][c];
			// counter--;
		}
	}

	if (b > 0 && b < WORLD_SIZE_Y - 1) {
		value += my_world[a][b - 1][c];
		value += my_world[a][b + 1][c];
	} else {
		if (b > 0) {
			value += my_world[a][b - 1][c];
			// counter--;
		}
		if (b < WORLD_SIZE_Y - 1) {
			value += my_world[a][b + 1][c];
			// counter--;
		}
	}

	if (c > 0 && c < WORLD_SIZE_Z - 1) {
		value += my_world[a][b][c - 1];
		value += my_world[a][b][c + 1];
	} else {
		if (c > 0) {
			value += my_world[a][b][c - 1];
			// counter--;
		}
		if (c < WORLD_SIZE_Z - 1) {
			value += my_world[a][b][c + 1];
			// counter--;
		}
	}

	value /= counter;

	return value;
}
