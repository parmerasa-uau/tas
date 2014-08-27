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
#include "tas_log.h"
#include "tas_demo_3.h"
#include "../platform.h"

#define MAT_SIZE 2048
// #define MAT_PACKAGES ((MAT_SIZE * MAT_SIZE) / 4) // 36 for 10
#define MAT_LINES_PER_THREAD (MAT_SIZE / 8)
#define MAT_PACKAGES ((MAT_SIZE * MAT_SIZE) / (MAT_LINES_PER_THREAD * MAT_LINES_PER_THREAD))
#define ECHO_MATRIZES 0

typedef struct matmul_args {
	int a, c, id;
} matmul_args_t;

// Source and target matrices
#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile int mat_a [MAT_SIZE][MAT_SIZE];
SHARED_VARIABLE(membase_uncached0) volatile int mat_b [MAT_SIZE][MAT_SIZE];
SHARED_VARIABLE(membase_uncached0) volatile int mat_c [MAT_SIZE][MAT_SIZE];
#else
int mat_a[MAT_SIZE][MAT_SIZE];
int mat_b[MAT_SIZE][MAT_SIZE];
int mat_c[MAT_SIZE][MAT_SIZE];
#endif

void * init(void * args) {
	struct matmul_args * my_data;
	my_data = (struct matmul_args *) args;

	int x, y;

#if TAS_INFO == 1
	PFL
	printf("START init [%i] %i %i %i \n", my_data->id, my_data->a, my_data->c, MAT_LINES_PER_THREAD);
	PFU
#endif

	for (x = 0; x < MAT_LINES_PER_THREAD; x++) {
		for (y = 0; y < MAT_LINES_PER_THREAD; y++) {
			mat_a[my_data->a + x][my_data->c + y] = (x * MAT_LINES_PER_THREAD + y) % 10;
			mat_b[my_data->a + x][my_data->c + y] = (x * MAT_LINES_PER_THREAD + y) % 10;
		}
	}

#if TAS_INFO == 1
	PFL
	printf("START init [%i] %i %i %i \n", my_data->id, my_data->a, my_data->c, MAT_LINES_PER_THREAD);
	PFU
#endif

	return NULL;
}

void * mult(void * args) {
	struct matmul_args * my_data;
	my_data = (struct matmul_args *) args;

#if TAS_INFO == 1
	PFL
	printf("START mult %i [ %i %i ] LPT %i\n", my_data->id, my_data->a, my_data->c, MAT_LINES_PER_THREAD);
	PFU
#endif

	if (1) {
		int a[MAT_LINES_PER_THREAD][MAT_SIZE], c[MAT_LINES_PER_THREAD][MAT_SIZE], result[MAT_LINES_PER_THREAD][MAT_LINES_PER_THREAD];
		int i, j, k;

		// "GET"
		for (j = 0; j < MAT_LINES_PER_THREAD; j++) {
			for (i = 0; i < MAT_SIZE; i++) {
				a[j][i] = mat_a[my_data->a + j][i];
				c[j][i] = mat_b[i][my_data->c + j];

#if TAS_DEBUG == 1
				printf("GET MAT_A[%i %i] to a[%i %i]: %i\n", my_data->a + j, i, j, i, a[j][i]);
#endif
			}
		}

		// MULT
		for (k = 0; k < MAT_LINES_PER_THREAD; k++) {
			for (j = 0; j < MAT_LINES_PER_THREAD; j++) {
				int w = 0;
				for (i = 0; i < MAT_SIZE; i++) {
					w += a[k][i] * c[j][i];
#if TAS_DEBUG == 1
					printf("   MULT [%i %i] %i * %i => %i\n", my_data->a + k, my_data->c + j, a[k][i], c[j][i], w);
#endif
				}
#if TAS_DEBUG == 1
				printf("MULT [%i %i] %i * %i => %i\n", my_data->a + k, my_data->c + j, a[k][i], c[j][i], w);
#endif
				result[k][j] = w;
			}
		}

		// "PUT"

		for (k = 0; k < MAT_LINES_PER_THREAD; k++) {
			for (j = 0; j < MAT_LINES_PER_THREAD; j++) {
				mat_c[my_data->a + k][my_data->c + j] = result[k][j];
			}
		}
	}

#if TAS_INFO == 1
	PFL
	printf("END mult %i [ %i %i ] LPT %i\n", my_data->id, my_data->a, my_data->c, MAT_LINES_PER_THREAD);
	PFU
#endif

	return NULL;
}

// Data structures for data parallel pattern
#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile matmul_args_t dp_mm_args_data[MAT_PACKAGES];
SHARED_VARIABLE(membase_uncached0) volatile void * dp_mm_args[MAT_PACKAGES];
SHARED_VARIABLE(membase_uncached0) volatile tas_dataparallel_t dp_mm = {
	(tas_runnable_t) init, dp_mm_args, MAT_PACKAGES};
#else
matmul_args_t dp_mm_args_data[MAT_PACKAGES];
void * dp_mm_args[MAT_PACKAGES];
tas_dataparallel_t dp_mm = { (tas_runnable_t) init, dp_mm_args, MAT_PACKAGES };
#endif

void demo_3_main_core_0() {
	int i, j;
	long my_mult, my_init;

	PFL
	printf("MAIN START\n");
	PFU

	PFL
	printf("Processing %ix%i matrix with %i pkg and %i lpt and %i threads\n",MAT_SIZE, MAT_SIZE, MAT_PACKAGES, MAT_LINES_PER_THREAD, tas_get_workers_available() + 1);
	PFU

	// Data Parallel Pattern
	for (i = 0; i < MAT_SIZE / MAT_LINES_PER_THREAD; i += 1) {
		for (j = 0; j < MAT_SIZE / MAT_LINES_PER_THREAD; j += 1) {
			int index = i * (MAT_SIZE / MAT_LINES_PER_THREAD) + j;

			dp_mm_args_data[index].a = i * MAT_LINES_PER_THREAD;
			dp_mm_args_data[index].c = j * MAT_LINES_PER_THREAD;
			dp_mm_args_data[index].id = index;
#if TAS_INFO == 1
			printf("Package %i: %i x %i with %i lines\n", index, i * 2, j * 2, MAT_LINES_PER_THREAD);
#endif

			dp_mm_args[index] = &(dp_mm_args_data[index]);
		}
	}

	// Init Skeleton
	tas_dataparallel_init(&dp_mm, tas_get_workers_available() + 1);

	// INIT
	logStart("Init");
	dp_mm.runnable = (tas_runnable_t) init;
	tas_dataparallel_execute(&dp_mm);
	my_init = logEnd("Init");

#if ECHO_MATRIZES == 1
	PFL
	printf("### MATRIX A\n");
	for (a = 0; a < MAT_SIZE; a++) {
		for (b = 0; b < MAT_SIZE; b++) {
			printf("%i ", mat_a[a][b]);
		}
		printf("\n");
	}
	PFU

	PFL
	printf("### MATRIX B\n");
	for (a = 0; a < MAT_SIZE; a++) {
		for (b = 0; b < MAT_SIZE; b++) {
			printf("%i ", mat_b[a][b]);
		}
		printf("\n");
	}
	PFU
#endif

	// MULT
	logStart("Mult");
	dp_mm.runnable = (tas_runnable_t) mult;
	tas_dataparallel_execute(&dp_mm);
	my_mult = logEnd("Mult");

#if ECHO_MATRIZES == 1
	PFL
	printf("### MATRIX C\n");
	for (a = 0; a < MAT_SIZE; a++) {
		for (b = 0; b < MAT_SIZE; b++) {
			printf("%i ", mat_c[a][b]);
		}
		printf("\n");
	}
	PFU
#endif

	// Finalize skeleton
	tas_dataparallel_finalize(&dp_mm);
	
	printf("DATA Processing %ix%i matrix with %i pkg and %i lpt and %i threads in %ul us and %ul us\n",MAT_SIZE, MAT_SIZE, MAT_PACKAGES, MAT_LINES_PER_THREAD, tas_get_workers_available() + 1, my_init , my_mult);

	PFL
	printf("MAIN END\n");
	PFU
}
