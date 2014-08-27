#include "fft.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include "tas_stdio.h"
#include "tas_log.h"
#include "tas.h"

#define N (64 * 2)
#define M (64 * 2)
#define L 16
#define STAGES 5
#define ITERATIONS 25
#define ATOA_WORKERS 4
#define FFT_LOG 0

int running = 1;

//memory for first set of matrices
complex_t** val_a_l_1[L];
complex_t** val_A_l_1[L];
complex_t** val_B_l_1[L];
complex_t** val_C_l_1[L];

complex_t* val_a_ln_1[L][N];
complex_t* val_A_ln_1[L][N];
complex_t* val_B_ln_1[L][N];
complex_t* val_C_ln_1[L][N];
complex_t* val_D_n_1[N];
complex_t* val_d_n_1[N];

complex_t val_a_lnm_1[L][N][M];
complex_t val_A_lnm_1[L][N][M];
complex_t val_B_lnm_1[L][N][M];
complex_t val_C_lnm_1[L][N][M];
complex_t val_D_nm_1[N][M];
complex_t val_d_nm_1[N][M];

//memory for second set of matrices
complex_t** val_a_l_2[L];
complex_t** val_A_l_2[L];
complex_t** val_B_l_2[L];
complex_t** val_C_l_2[L];

complex_t* val_a_ln_2[L][N];
complex_t* val_A_ln_2[L][N];
complex_t* val_B_ln_2[L][N];
complex_t* val_C_ln_2[L][N];
complex_t* val_D_n_2[N];
complex_t* val_d_n_2[N];

complex_t val_a_lnm_2[L][N][M];
complex_t val_A_lnm_2[L][N][M];
complex_t val_B_lnm_2[L][N][M];
complex_t val_C_lnm_2[L][N][M];
complex_t val_D_nm_2[N][M];
complex_t val_d_nm_2[N][M];

typedef struct {
	complex_t*** val_a;
	complex_t*** val_A;
	complex_t*** val_B;
	complex_t*** val_C;
	complex_t** val_D;
	complex_t** val_d;
} pipeline_cache_t;

pipeline_cache_t cache[2];

typedef struct {
	tas_pipeline_inout_t * _inout;
	int from;
	int to;
} a_to_A_dp_args_t;

a_to_A_dp_args_t a_to_A_dp_args[ATOA_WORKERS];

// void * (*stages[STAGES])(void *);


#if TAS_POSIX == 0
SHARED_VARIABLE(membase_uncached0) volatile ticketlock_t lock_random;
#else
pthread_mutex_t lock_random = PTHREAD_MUTEX_INITIALIZER;
#endif

#define rando_N 25

// Random number between 0 and 1
// http://de.wikipedia.org/wiki/Mersenne-Twister -- TT800
double rando() {
	const int rando_M = 7;
	const unsigned int A[2] = { 0, 0x8ebfd028 };

	ticket_lock(&lock_random); // ID=rando_lock
	static unsigned int y[rando_N];
	static int index = rando_N + 1;

	if (index >= rando_N) {
		int k;
		if (index > rando_N) {
			unsigned r = 9, s = 3402;
			for (k = 0; k < rando_N; ++k) {
				r = 509845221 * r + 3;
				s *= s + 1;
				y[k] = s + (r >> 10);
			}
		}
		for (k = 0; k < rando_N - rando_M; ++k)
			y[k] = y[k + rando_M] ^ (y[k] >> 1) ^ A[y[k] & 1];
		for (; k < rando_N; ++k)
			y[k] = y[k + (rando_M - rando_N)] ^ (y[k] >> 1) ^ A[y[k] & 1];
		index = 0;
	}

	unsigned e = y[index++];
	ticket_unlock(&lock_random); // ID=rando_lock

	e ^= (e << 7) & 0x2b5b2500;
	e ^= (e << 15) & 0xdb8b0000;
	e ^= (e >> 16);

	return (double) e / UINT_MAX;
}

void create_matrices() {
	pipeline_cache_t * _cache = &cache[0];

	int n, l;
	_cache->val_a = val_a_l_1;
	_cache->val_A = val_A_l_1;
	_cache->val_B = val_B_l_1;
	_cache->val_C = val_C_l_1;
	for (l = 0; l < L; l++) {
		_cache->val_a[l] = val_a_ln_1[l];
		_cache->val_A[l] = val_A_ln_1[l];
		_cache->val_B[l] = val_B_ln_1[l];
		_cache->val_C[l] = val_C_ln_1[l];
		for (n = 0; n < N; n++) {
			_cache->val_a[l][n] = val_a_lnm_1[l][n];
			_cache->val_A[l][n] = val_A_lnm_1[l][n];
			_cache->val_B[l][n] = val_B_lnm_1[l][n];
			_cache->val_C[l][n] = val_C_lnm_1[l][n];
		}
	}

	_cache->val_d = val_d_n_1;
	_cache->val_D = val_D_n_1;
	for (n = 0; n < N; n++) {
		_cache->val_d[n] = val_d_nm_1[n];
		_cache->val_D[n] = val_D_nm_1[n];
	}

	_cache = &cache[1];

	_cache->val_a = val_a_l_2;
	_cache->val_A = val_A_l_2;
	_cache->val_B = val_B_l_2;
	_cache->val_C = val_C_l_2;
	for (l = 0; l < L; l++) {
		_cache->val_a[l] = val_a_ln_2[l];
		_cache->val_A[l] = val_A_ln_2[l];
		_cache->val_B[l] = val_B_ln_2[l];
		_cache->val_C[l] = val_C_ln_2[l];
		for (n = 0; n < N; n++) {
			_cache->val_a[l][n] = val_a_lnm_2[l][n];
			_cache->val_A[l][n] = val_A_lnm_2[l][n];
			_cache->val_B[l][n] = val_B_lnm_2[l][n];
			_cache->val_C[l][n] = val_C_lnm_2[l][n];
		}
	}

	_cache->val_d = val_d_n_2;
	_cache->val_D = val_D_n_2;
	for (n = 0; n < N; n++) {
		_cache->val_d[n] = val_d_nm_2[n];
		_cache->val_D[n] = val_D_nm_2[n];
	}
}

void init_matrices(pipeline_cache_t * _cache) {
	int n, m, l;
	for (l = 0; l < L; l++) {
		for (n = 0; n < N; n++) {
			for (m = 0; m < M; m++) {
				_cache->val_a[l][n][m].real = 0.0f;
				_cache->val_a[l][n][m].imag = 0.0f;
				_cache->val_B[l][n][m].real = 0.0f;
				_cache->val_B[l][n][m].imag = 0.0f;
				_cache->val_A[l][n][m].real = 0.0f;
				_cache->val_A[l][n][m].imag = 0.0f;
				_cache->val_C[l][n][m].real = 0.0f;
				_cache->val_C[l][n][m].imag = 0.0f;
			}
		}
	}

	for (n = 0; n < N; n++) {
		for (m = 0; m < M; m++) {
			_cache->val_d[n][m].real = 0.0f;
			_cache->val_d[n][m].imag = 0.0f;
			_cache->val_D[n][m].real = 0.0f;
			_cache->val_D[n][m].imag = 0.0f;
		}
	}
}

void * create_fft_input(void * _inout) {
	#if FFT_LOG == 1
		logStart("create_fft_input");
	#endif
	/*PFL
	printf("+ create_fft_input\n");
	PFU*/

	pipeline_cache_t * _cache = ((tas_pipeline_inout_t *) _inout)->input;

	int n, m, l;
	for (l = 0; l < L; l++) {
		for (n = 0; n < N; n++) {
			for (m = 0; m < M; m++) {
				// float foo = rando();
				float foo = (float) rand() / (float) RAND_MAX;
				_cache->val_a[l][n][m].real = foo;
						// (float) rand() / (float) RAND_MAX;
				_cache->val_a[l][n][m].imag = foo;
						// (float) rand() / (float) RAND_MAX;
				_cache->val_B[l][n][m].real = foo;
						// (float) rand() / (float) RAND_MAX;
				_cache->val_B[l][n][m].imag = foo;
						// (float) rand() / (float) RAND_MAX;
			}
		}
	}

	/*PFL
	printf("- create_fft_input\n");
	PFU*/

	#if FFT_LOG == 1
		logEnd("create_fft_input");
	#endif

	return(NULL);
}

complex_t cplx_mult(complex_t one, complex_t two) {
	complex_t retval;
	retval.imag = one.imag * two.imag;
	retval.real = one.real * two.real;
	return retval;
}

complex_t cplx_add(complex_t one, complex_t two) {
	complex_t retval;
	retval.imag = one.imag + two.imag;
	retval.real = one.real + two.real;
	return retval;
}

void complex_product(complex_t ***A, complex_t ***B, complex_t ***C) {
	int n, m, l;
	for (n = 0; n < N; n++) {
		for (m = 0; m < M; m++) {
			for (l = 0; l < L; l++) {
				C[l][n][m] = cplx_mult(A[l][n][m], B[l][n][m]);
			}
		}
	}
}

void layer_sum(complex_t ***C, complex_t** D) {
	int n, m, l;
	for (n = 0; n < N; n++) {
		for (m = 0; m < M; m++) {
			for (l = 0; l < L; l++) {
				D[n][m] = cplx_add(D[n][m], C[l][n][m]);
			}
		}
	}
}

void * a_to_A_sub(void * my_inout) {
	a_to_A_dp_args_t * my_args = (a_to_A_dp_args_t * ) my_inout;

	pipeline_cache_t * input = my_args->_inout->input;
	pipeline_cache_t * output = my_args->_inout->output;

	// int counter = 0;
	int l,n,m;

	for (l = my_args->from; l <= my_args->to; l++) { //<----
		FFT2D(input->val_a[l], N, M, 0, output->val_A[l]);
		//copy input matrix B for second pipeline step
		for(n=0; n<N; n++) {
			for(m=0; m<M; m++) {
				output->val_B[l][n][m] = input->val_B[l][n][m];
			}
		}
	}

	return(NULL);
}

void * a_to_A_dp_args_pointers[ATOA_WORKERS];
tas_dataparallel_t a_to_A_dp = { (tas_runnable_t) a_to_A_sub, a_to_A_dp_args_pointers, ATOA_WORKERS };


/** A is 2D FFT on every layer of a */
void * a_to_A(void * _inout) {
	#if FFT_LOG == 1
		logStart("a_to_A");
	#endif

	tas_dataparallel_execute(& a_to_A_dp); // ID=tas_pipeline_step_tp

	#if FFT_LOG == 1
		logEnd("a_to_A");
	#endif

	return(NULL);
}

/** C is element wise complex product of A and B (A*B) */
void * AB_to_C(void * _inout) {
	#if FFT_LOG == 1
		logStart("AB_to_C");
	#endif

	/* PFL
	printf("+ AB_to_C\n");
	PFU */

	pipeline_cache_t * input = ((tas_pipeline_inout_t *) _inout)->input;
	pipeline_cache_t * output = ((tas_pipeline_inout_t *) _inout)->output;

	int counter = 0;

	complex_product(input->val_A, input->val_B, output->val_C); //<----
	counter++;

	/* PFL
	printf("- AB_to_C\n");
	PFU */

	#if FFT_LOG == 1
		logEnd("AB_to_C");
	#endif

	return(NULL);
}

/** D is sum over all layers of C */
void * C_to_D(void * _inout) {
	#if FFT_LOG == 1
		logStart("C_to_D");
	#endif

	/* PFL
	printf("+ C_to_D\n");
	PFU */

	pipeline_cache_t * input = ((tas_pipeline_inout_t *) _inout)->input;
	pipeline_cache_t * output = ((tas_pipeline_inout_t *) _inout)->output;

	int counter = 0;

	layer_sum(input->val_C, output->val_D); //<----
	counter++;

	/* PFL
	printf("- C_to_D\n");
	PFU */

	#if FFT_LOG == 1
		logEnd("C_to_D");
	#endif
	return(NULL);
}

/** d is 2D IFFT of D */
void * D_to_d(void * _inout) {
	#if FFT_LOG == 1
		logStart("D_to_d");
	#endif

	/* PFL
	printf("+ D_to_d\n");
	PFU */

	pipeline_cache_t * input = ((tas_pipeline_inout_t *) _inout)->input;
	pipeline_cache_t * output = ((tas_pipeline_inout_t *) _inout)->output;

	int counter = 0;

	FFT2D(input->val_D, N, M, 1, output->val_d); //<----
	counter++;

	/* PFL
	printf("- D_to_d\n");
	PFU */
	#if FFT_LOG == 1
		logEnd("D_to_d");
	#endif

	return(NULL);
}

// Data structures for task parallel pattern
#if TAS_POSIX == 0
// MUST BE TESTED
SHARED_VARIABLE(membase_uncached0) volatile tas_pipeline_inout_t io = {& cache[0], & cache[1]};
SHARED_VARIABLE(membase_uncached0) volatile void * task_parallelism_args[STAGES] = {
		(void *) &io, (void *) &io, (void *) &io, (void *) &io, (void *) &io
};
SHARED_VARIABLE(membase_uncached0) volatile tas_runnable_t task_parallelism_runnables[STAGES] = {
		(tas_runnable_t) &create_fft_input,
		(tas_runnable_t) &a_to_A, (tas_runnable_t) &AB_to_C,
		(tas_runnable_t) &C_to_D, (tas_runnable_t) &D_to_d
};
SHARED_VARIABLE(membase_uncached0) volatile tas_taskparallel_t task_parallelism = { task_parallelism_runnables, task_parallelism_args, STAGES };
#else
tas_pipeline_inout_t io = {& cache[0], & cache[1]};
void * task_parallelism_args[STAGES] = {
		(void *) &io, (void *) &io, (void *) &io, (void *) &io, (void *) &io
};
tas_runnable_t task_parallelism_runnables[STAGES] = {
		(tas_runnable_t) &create_fft_input,
		(tas_runnable_t) &a_to_A, (tas_runnable_t) &AB_to_C,
		(tas_runnable_t) &C_to_D, (tas_runnable_t) &D_to_d
};
tas_taskparallel_t task_parallelism = { task_parallelism_runnables, task_parallelism_args, STAGES };
#endif


/** Execute a parallel pipeline instance */
void demo_fft_init() {
	int i;

	logStart("FFT-Demo");
	PFL
	printf("Starting FFT test\n");
	PFU

#if TAS_POSIX == 0
	ticket_init(&lock_random);
#endif

	create_matrices();
	init_matrices(&cache[0]);
	init_matrices(&cache[1]);

	for(i = 0; i < ATOA_WORKERS; i++) {
		a_to_A_dp_args[i]._inout = &io;
		a_to_A_dp_args[i].from = i * (L / ATOA_WORKERS);
		a_to_A_dp_args[i].to = (i + 1) * (L / ATOA_WORKERS) - 1;
		a_to_A_dp_args_pointers[i] = &(a_to_A_dp_args[i]);
	}

	PFL
	printf("Matrices initialized\n");
	PFU

	logStart("FFT-Demo_pl");
	tas_taskparallel_init(&task_parallelism, 5);
	tas_dataparallel_init(&a_to_A_dp, 4);

	logStart("FFT-Demo_pl_ex");
	tas_pipeline_execute(&task_parallelism, ITERATIONS, &io); // ID=tas_pipeline_fft
	logEnd("FFT-Demo_pl_ex");

	tas_taskparallel_finalize(&task_parallelism);
	tas_dataparallel_finalize(&a_to_A_dp);
	logEnd("FFT-Demo_pl");

	logEnd("FFT-Demo");
}

