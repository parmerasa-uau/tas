#ifndef TAS_H
#define TAS_H

#define TAS_MAIN_IS_WORKER 1
#define TAS_POSIX 1
#define TAS_MAX_WORKERS 48

#define TAS_STOPWATCH_ACTIVE 1

#define TAS_MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define TAS_MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

// #define TAS_STATISTICS 0
#define TAS_DEBUG 0
#define TAS_INFO 0

#if TAS_POSIX == 1
#include <pthread.h>
#include "tas_pthread_compat.h"
#else
#include "../kernel_lib/kernel_lib.h"
#endif

/** Data type for runnable function which are executed by a Parallel Design Pattern */
typedef void (*tas_runnable_t)(void * args);

/** Data type for the thread administration data structure */
typedef struct tas_thread_struct tas_thread_t;

/** Abstract administration data type for all Parallel Design Patterns */
typedef struct tas_abstract_struct tas_abstract_t;

struct tas_thread_struct {
	int thread_id;
	void * args;
	tas_runnable_t runnable;
	tas_abstract_t * parent;

	char do_shutdown;
	char work_assigned;
	char pattern_assigned;

#if TAS_POSIX == 1
	pthread_barrier_t pattern_assigned_barrier_idle;
	pthread_barrier_t pattern_assigned_barrier;
#else
	barrier_t  pattern_assigned_barrier_idle;
	barrier_t  pattern_assigned_barrier;
#endif
};

struct tas_abstract_struct {
	int nr_threads; // Number of worker threads
	int workers [TAS_MAX_WORKERS];
#if TAS_POSIX == 1
	pthread_barrier_t work_available_barrier; // Work is available
	pthread_barrier_t work_done_barrier; // Work is done
#else
	barrier_t  work_available_barrier; // Work is available
	barrier_t  work_done_barrier; // Work is done
#endif
};

/** Data structure for a Task Parallelism Parallel Design Pattern */
typedef struct tas_taskparallel_struct {
	tas_runnable_t * runnables;
	void ** args;
	int nr_runnables;

	tas_abstract_t base;
} tas_taskparallel_t;

/** Data structure for a Data Parallel Parallel Design Pattern */
typedef struct tas_dataparallel_struct {
	tas_runnable_t runnable;
	void ** args;
	int nr_args;

	tas_abstract_t base;
} tas_dataparallel_t;

/** Data structure for double-buffering in Parallel Pipeline Pattern */
typedef struct {
	void * input; // pipeline_cache_t
	void * output; // pipeline_cache_t
} tas_pipeline_inout_t;

void tas_init(int workers);

void tas_finalize();

/** Initialize Task Parallelism Pattern */
void tas_taskparallel_init(tas_taskparallel_t * instance, int threads);

/** Initialize Data Parallel Pattern */
void tas_dataparallel_init(tas_dataparallel_t * instance, int threads);

/** Clean up Task Parallelism Pattern */
void tas_taskparallel_finalize(tas_taskparallel_t * instance);

/** Clean up Data Parallel Pattern */
void tas_dataparallel_finalize(tas_dataparallel_t * instance);

/** Execute Task Parallel Pattern */
void tas_taskparallel_execute(tas_taskparallel_t * instance);

/** Execute Task Parallel Pattern with restricted tasks (first = 0, last = tasks.size - 1) */
void tas_taskparallel_execute_with_limits(tas_taskparallel_t * instance, int first, int last);

/** Execute Pipeline Parallel Pattern */
void tas_pipeline_execute(tas_taskparallel_t * task_parallelism, int iterations, tas_pipeline_inout_t * io);

/** Execute Data Parallelism Pattern */
void tas_dataparallel_execute(tas_dataparallel_t * instance);

/** Return number of available workers */
int tas_get_workers_available();

/** Code for Workers */
void * tas_thread(void * parameters_void);

#if TAS_POSIX == 0
extern volatile tas_thread_t worker_threads_data[TAS_MAX_WORKERS];
#else
extern tas_thread_t worker_threads_data[TAS_MAX_WORKERS];
#endif

#endif // TAS_H
