#include "tas.h"

// #include "TimeStamp.h"

#if TAS_STOPWATCH_ACTIVE == 1
#if TAS_POSIX == 1

// #include <linux/time.h>
#include <time.h>
#include <stddef.h>
#include <stdio.h>


#define NANO 1000000000LL

#else
#include "../kernel_lib/kernel_lib.h"
#endif
#endif

#if TAS_STOPWATCH_ACTIVE == 1
#if TAS_POSIX == 1
// struct timeval start, end, now;
long start, end, now;
#else
long start, end, now;
#endif
#endif

/** Returns timestamp in nanoseconds - multiply by 1,000,000,000 to get seconds */
long getTimestampHP() {
#if TAS_STOPWATCH_ACTIVE == 1
#if TAS_POSIX == 1
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts );
	// clock_gettime(CLOCK_MONOTONIC, &ts );

	return (ts.tv_nsec + ts.tv_sec * NANO);
#else
	return get_time_base();
#endif
#else
	return 0;
#endif
}

/** Returns timestamp in microseconds - multiply by 1000000 to get seconds */
long getTimestamp() {
#if TAS_STOPWATCH_ACTIVE == 1
#if TAS_POSIX == 1
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts );
	// clock_gettime(CLOCK_MONOTONIC, &ts );

	return (ts.tv_nsec + ts.tv_sec * NANO) / 1000;
#else
	return get_time_base();
#endif
#else
	return 0;
#endif
}

/** Starts time measure */
void start_stopwatch() {
#if TAS_STOPWATCH_ACTIVE == 1
#if TAS_POSIX == 1
	// gettimeofday(&start, NULL);
	start = getTimestamp();
#else
	start = get_time_base();
#endif
#endif
}

/** Returns the time in microseconds (1 * 10^-6) since last call of start_stopwatch */
long stop_stopwatch() {
#if TAS_STOPWATCH_ACTIVE == 1
#if TAS_POSIX == 1
	end = getTimestamp();
	/* gettimeofday(&end, NULL);

	long mtime, seconds, useconds;

	seconds = end.tv_sec - start.tv_sec;
	useconds = end.tv_usec - start.tv_usec;

	mtime = (seconds) * 1000000 + useconds;  */

	return end - start;
#else
	end = get_time_base();
	return end - start;
#endif
#else
	return 0;
#endif
}


