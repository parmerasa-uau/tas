# tas/tas_demo_3.c tas/tas_demo_1.c tas/tas_demo_2.c
# SOURCE_FILES = main.c tas/tas.c tas/tas_log.c tas/tas_stopwatch.c tas/tas_demo_fft_par.c tas/fft.c 

# Files for 3D Wayfind
# SOURCE_FILES = main.c tas/tas.c tas/tas_log.c tas/tas_stopwatch.c tas/tas_demo_7.c tas/algo_jacobi_par.c tas/data.c tas/algo_gauss-seidel_par.c

# DEBUG:  -g -p
# RELEASE: -O3
all: ${SOURCE_FILES}
	gcc -O3 -g -p -o tas_example_x86 -Wall -pthread  ${SOURCE_FILES} -lm -lrt

clean:
	rm -f tas_example_x86