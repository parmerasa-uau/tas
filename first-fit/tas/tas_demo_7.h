void demo_7_main_core_0();

#include "data.h"
#include "../platform.h"

extern double world[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z];
extern char global_obstacles[WORLD_SIZE_X][WORLD_SIZE_Y][WORLD_SIZE_Z];

extern int position[3];
extern int goal[3];

#define THREADS TOTAL_PROC_NUM
#define ITERATIONS 256

// Gauss-Seidel SEQUENTIAL
// #include "algo_gauss-seidel_seq.h"
// #define promote_world(foo) algo_gaussseidel_seq(foo)

// Gauss-Seidel PARALLEL -- no barrier
// #include "algo_gauss-seidel_par.h"
// #define promote_world(foo) algo_gaussseidel_par(foo)

// Gauss-Seidel PARALLEL -- with barrier
// #include "algo_gauss-seidel_par.h"
// #define promote_world(foo) algo_gaussseidel_par_barrier(foo)

// Gauss-Seidel PARALLEL -- with barrier
// #include "algo_gauss-seidel_par.h"
// #define promote_world(foo) algo_gaussseidel_par_barrier_mw(foo)

// Jacobi SEQUENTIAL
// #include "algo_gauss-seidel_seq.h"
// #define promote_world(foo) algo_jacobi_seq(foo)

// Jacobi PARALLEL -- no barrier
// #include "algo_gauss-seidel_par.h"
// #define promote_world(foo) algo_jacobi_par(foo)

// Jacobi PARALLEL -- with barrier
// #include "algo_gauss-seidel_par.h"
// #define promote_world(foo) algo_jacobi_par_barrier(foo)
// #define promote_world(foo) algo_jacobi_par_barrier_2(foo)

// Jacobi PARALLEL -- with barrier M/W
// #include "algo_jacobi_par.h"
// #define promote_world(foo) algo_jacobi_par_barrier_mw(foo)
// #define promote_world(foo) algo_jacobi_par_barrier_mw_2(foo)

// Jacobi PARALLEL -- with skeletons ==> BOTH WORK!
// #include "algo_jacobi_par.h"
// #define promote_world(foo, bar) algo_jacobi_par_skel_simple(foo, bar)
// #define promote_world(foo, bar) algo_jacobi_par_skel_complex(foo, bar)

// Gauss-Seidel PARALLEL -- Skeleton ==> WORKS!
#define COMPARTMENTS_PER_AXIS 4
#include "algo_gauss-seidel_par.h"
#define promote_world(foo, bar) algo_gaussseidel_par_skel(foo, bar)

