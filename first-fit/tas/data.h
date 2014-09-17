#define WORLD_SIZE_X 64
#define WORLD_SIZE_Y 64
#define WORLD_SIZE_Z 256

#define WEIGHT_OBSTACLE 0.0
#define WEIGHT_INIT 0.0
#define WEIGHT_SINK 1.0

int * getPosition();

void setPosition(int a, int b, int c);

int * getGoal();

void setGoal(int a, int b, int c);

char isGoal(int a, int b, int c);

unsigned char getHeightmapValue(int x, int y);

char isObstacle(int x, int y, int z);

double getAverage(double my_world[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z], int a, int b, int c);
