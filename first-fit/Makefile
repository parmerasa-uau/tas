# tas/tas_demo_3.c tas/tas_demo_1.c tas/tas_demo_2.c
# SOURCE_FILES = main.c tas/tas.c tas/tas_log.c tas/tas_stopwatch.c tas/tas_demo_fft_par.c tas/fft.c 
SOURCE_FILES = main.c tas/tas.c tas/tas_log.c tas/tas_stopwatch.c tas/tas_demo_3.c

# DEBUG:  -g -p
all: ${SOURCE_FILES}
	gcc -O3 -o tas_example_x86 -Wall -pthread  ${SOURCE_FILES} -lm -lrt

clean:
	rm -f tas_example_x86