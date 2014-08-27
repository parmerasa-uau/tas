#ifndef TAS_STDIO_H
#define TAS_STDIO_H

#include "tas.h"
#include "tas_pthread_compat.h"

#if TAS_POSIX == 0
#include "../kernel_lib/kernel_lib.h"
extern volatile ticketlock_t lock_main_printf;
#else
#include <pthread.h>
extern pthread_mutex_t lock_main_printf;
#endif

#define PFL ticket_lock(&lock_main_printf);
#define PFU ticket_unlock(&lock_main_printf);

#endif // TAS_STDIO_H
