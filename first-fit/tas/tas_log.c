#include <stdio.h>
#include <stdlib.h>

#include "tas_log.h"
#include "tas.h"
#include "tas_stdio.h"

#include "tas_stopwatch.h"

#define DO_LOG_START 1
#define DO_LOG_END  1

long timestack[255];
int timestack_pointer = 0;

void logStart(char * text) {
#if (DO_LOG_START || DO_LOG_END)
	PFL
	timestack[timestack_pointer] = getTimestamp();
	timestack_pointer++;

	if (timestack_pointer > 254) {
		printf("Recursion too deep - EXIT\n");
		exit(1);
	}

	if (DO_LOG_START)
		printf("%d LOG START | %s\n", timestack_pointer, text);
	PFU
#endif
}

long logEnd(char * text) {
#if (DO_LOG_START || DO_LOG_END)
	PFL

	timestack_pointer--;
	long duration = 0;

	if (timestack_pointer < 0) {
		printf("logEnd called too often - EXIT\n");
		exit(1);
	}

	if (DO_LOG_END) {
		duration = getTimestamp() - timestack[timestack_pointer];
#if TAS_POSIX == 1
		printf("%d LOG END   | %s | %lu us\n", timestack_pointer + 1, text, duration);
#else
		printf("%d LOG END   | %s | %i us\n", timestack_pointer + 1, text, duration);
#endif
	}

	PFU

	return duration;
#endif
	return -1;
}

void logStartA(char * text, long a) {
#if (DO_LOG_END || DO_LOG_START)
#if TAS_POSIX == 1
	char buffer[255];
	sprintf(buffer, text, a);
	logStart(buffer);
#else
	logEnd(text);
#endif
#endif
}

long logEndA(char * text, long a) {
#if (DO_LOG_END || DO_LOG_START)
#if TAS_POSIX == 1
	char buffer[255];
	sprintf(buffer, text, a);
	return logEnd(buffer);
#else
	logEnd(text);
#endif
#endif
	return -1;
}

/* void logEnd(char * text, long duration) {
 printf("LOG END %s [%d ms]", text, duration);
 } */

// void log(char * text);
