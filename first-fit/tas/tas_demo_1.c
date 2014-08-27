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
#include "tas_demo_1.h"
#include "../platform.h"


void demo_1_main_core_0() {

	PFL
	printf("TEST\n");
	PFU
}
