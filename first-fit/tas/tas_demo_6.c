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
#include "tas_demo_6.h"
#include "tas_stopwatch.h"
#include "../platform.h"

#define VECTOR_SIZE (4096 * 4)
#define MODE_READ 1
#define MODE_PROCESS 2
#define MODE_WRITE 4
#define COMPUTATION 3

typedef struct perf_args {
	int id, length_read, mode, length_write, length_process;
} perf_args_t;

// Source and target matrices
#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile int vector [TOTAL_PROC_NUM][VECTOR_SIZE];
#else
int vector[TOTAL_PROC_NUM][VECTOR_SIZE];
#endif

void init() {
	int i, j, k = 1;

	for (i = 0; i < TOTAL_PROC_NUM; i++) {
		for (j = 0; j < VECTOR_SIZE; j++) {
			vector[i][j] = k;
			k++;
		}
	}
}

void * get_work_put(void * args) {
	struct perf_args * my_data;
	my_data = (struct perf_args *) args;

	int id = my_data->id;
	int length_read = my_data->length_read;
	int length_process = my_data->length_process;
	int length_write = my_data->length_write;

	int data[VECTOR_SIZE];

	int i;

#if TAS_INFO == 1
	// PFL
	printf(" -- START get_work_put on core %i: R=%i X=%i W=%i\n", id, length_read, length_process, length_write);
	// PFU
#endif


	for (i = 0; i < length_read; i++) {
		data[i] = vector[id][i];
	}

	for (i = 0; i < length_process; i++) {
		// int testing = 4711;
		int a = data[i];
		int b = data[(i + 1042 * id) % length_read];
		int c = data[(i + 2756 * id) % length_read];
#if COMPUTATION >= 4
		int d = data[(i + 6132 * id) % length_read];
#endif
#if COMPUTATION >= 5
		int e = data[(i + 4509 * id) % length_read];
#endif
		/* int f = data[(i + 464) % length_read];
		 int g = data[(i + 1822) % length_read];
		 int h = data[(i + 7482) % length_read]; */

		int result = 0;

		result += a * a;
		result += a * b;
		result += a * c;
#if COMPUTATION >= 4
		result += a * d;
#endif
		/* result += a * e;
		 result += a * f;
		 result += a * g;
		 result += a * h; */

		result += b * b;
		result += b * c;
#if COMPUTATION == 4
		result += b * d;
#endif
#if COMPUTATION >= 5
		result += b * e;
#endif
		/* result += b * f;
		 result += b * g;
		 result += b * h; */

		result += c * c;
#if COMPUTATION >= 4
		result += c * d;
#endif
#if COMPUTATION >= 5
		result += c * e;
#endif
		 
		 /*result += c * f;
		 result += c * g;
		 result += c * h; */

#if COMPUTATION >= 4
		result += d * d;
#endif
#if COMPUTATION >= 5
		result += d * e;
#endif
		 /* result += d * f;
		 result += d * g;
		 result += d * h; */

#if COMPUTATION >= 5
		result += e * e;
#endif

		 /* result += e * f;
		 result += e * g;
		 result += e * h;

		 result += f * f;
		 result += f * g;
		 result += f * h;

		 result += g * g;
		 result += g * h;

		 result += h * h; */

		/* for (i = 0; i < length_read; i++) {
		 data[i] = data[i] + data[(i + 1) % length_read]
		 + data[(i + 2) % length_read]
		 + data[(i + 3) % length_read]; */

		data[i] = result;
		// testing = 4712;
	}
	

	for (i = 0; i < length_write; i++) {
		vector[id][i] = data[i];
	}

#if TAS_INFO == 1
	// PFL
	printf(" -- END get_work_put on core %i: R=%i X=%i W=%i \n", id, length_read, length_process, length_write);
	// PFU
#endif

	return NULL;
}

// Data structures for data parallel pattern
#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile perf_args_t dp_perf_args_data[TOTAL_PROC_NUM];
SHARED_VARIABLE(membase_uncached0) volatile void * dp_perf_args[TOTAL_PROC_NUM];
SHARED_VARIABLE(membase_uncached0) volatile tas_dataparallel_t dp_perf = {
	(tas_runnable_t) get_work_put, dp_perf_args, TOTAL_PROC_NUM};
#else
perf_args_t dp_perf_args_data[TOTAL_PROC_NUM];
void * dp_perf_args[TOTAL_PROC_NUM];
tas_dataparallel_t dp_perf = { (tas_runnable_t) get_work_put, dp_perf_args,
		TOTAL_PROC_NUM };
#endif


void bubblesort(long * array, int length) {
	int i, j;
	for (i = 0; i < length - 1; ++i) {

		for (j = 0; j < length - i - 1; ++j) {
			if (array[j] > array[j + 1]) {
				int tmp = array[j];
				array[j] = array[j + 1];
				array[j + 1] = tmp;
			}
		}
	}
}


void demo_6_main_core_0() {
	int i, lr, lp, lw, m, j;

	// int length[] = { 0, 4, 8, 16, 32,  64, 128, 256, 512, 1024, 2048, 4096, 4096 * 2};
	// int length_count = 13;

	int length[] = { 0, 8, 32, 128, 512, 2048, 4096 * 2};
	int length_count = 7;

	// int length[] = { 0, 16, 128, 1024, 4096 };
	// int length_count = 5;

	int modes[] = { MODE_READ | MODE_WRITE | MODE_PROCESS };
	int modes_count = 1;

	int iterations = 128; // 512; // 1024

	PFL
	printf("MAIN START\n");
	PFU

	// Init vectors with data
	init();

	int cores = 1;
	for (cores = 1; cores <= TOTAL_PROC_NUM; cores++) {
	// for (cores = 4; cores <= 4; cores++) {

		printf("###### NOW WITH %i CORES #####\n", cores);

		// all data
		long data[length_count][length_count][length_count][modes_count][iterations];

		// Init Skeleton
		tas_dataparallel_init(&dp_perf, cores);
		dp_perf.nr_args = cores;

		for (j = 0; j < iterations; j++) {
			for (lr = 0; lr < length_count; lr++) {
				for (lp = 0; lp <= lr; lp++) {
					for (lw = 0; lw <= lp; lw++) {
						for (m = 0; m < modes_count; m++) {

							long duration;

							// Data Parallel Pattern
							for (i = 0; i < TOTAL_PROC_NUM; i++) {
								dp_perf_args_data[i].id = i;
								dp_perf_args_data[i].length_read = length[lr];
								dp_perf_args_data[i].length_process =
										length[lp];
								dp_perf_args_data[i].length_write = length[lw];
								dp_perf_args_data[i].mode = modes[m];
								dp_perf_args[i] = &dp_perf_args_data[i];
							}

							// INIT
							long start = getTimestamp();
							// logStart("Execute");
							tas_dataparallel_execute(&dp_perf);
							long stop = getTimestamp();
							// duration = logEnd("Execute");
							duration = stop - start;

							if(length[lr] == 512 && length[lp] == 512 && length[lw] == 512 && cores <= 4) {
								printf(
									"INTERMEDIATE cores %i read %i process %i write %i mode %i iteration %i duration %i\n",
									cores, length[lr], length[lp], length[lw],
									modes[m], j, duration);
							}

							data[lr][lp][lw][m][j] = duration;
						}
					}
				}
			}
			if(j % 16 == 0)
				printf("ITERATION %i of %i\n", j, iterations);
		}

		for (lr = 0; lr < length_count; lr++) {
			for (lp = 0; lp <= lr; lp++) {
				for (lw = 0; lw <= lp; lw++) {
					for (m = 0; m < modes_count; m++) {
						// printf("\n");
						// printf("Analysis for %i / %i: \n", length[lr], length[lw]);

						double sum = 0;
						for (j = 0; j < iterations; j++)
							sum += data[lr][lp][lw][m][j];
						double avg = sum / iterations;
						// printf("Average: %f\n", avg);

						sum = 0;
						for (j = 0; j < iterations; j++)
							sum += (avg - data[lr][lp][lw][m][j])
									* (avg - data[lr][lp][lw][m][j]);
						double var = sum / iterations;

						bubblesort(data[lr][lp][lw][m], iterations);

						sum = 0;
						int skipped_elements = 0.15 * iterations;
						// printf("Skipping %i elements\n", skipped_elements);
						for (j = 0 + skipped_elements;
								j < iterations - skipped_elements; j++)
							sum += data[lr][lp][lw][m][j];
						double trimmed_avg = sum
								/ (iterations - 2 * skipped_elements);
						// printf("Trimmed average: %f\n", trimmed_avg);

						sum = 0;
						for (j = 0 + skipped_elements;
								j < iterations - skipped_elements; j++)
							sum += (trimmed_avg - data[lr][lp][lw][m][j])
									* (trimmed_avg - data[lr][lp][lw][m][j]);
						double trimmed_var = sum
								/ (iterations - 2 * skipped_elements);

						int read = 0, process = 0, write = 0, mode = modes[m];

						if (mode & MODE_READ)
							read = length[lr];
						if (mode & MODE_PROCESS)
							process = length[lp];
						if (mode & MODE_WRITE)
							write = length[lw];

						printf(
								"DATA cores %i mode %i r %i x %i w %i avg %f t_avg %f var %f t_var %f\n",
								cores, mode, read, process, write, avg,
								trimmed_avg, var, trimmed_var);
					}
				}
			}
		}

		// Finalize skeleton
		tas_dataparallel_finalize(&dp_perf);
	}

	PFL
	printf("MAIN END\n");
	PFU
}
