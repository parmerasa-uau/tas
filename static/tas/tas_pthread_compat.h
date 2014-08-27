#ifndef TAS_PTHREAD_COMPAT_H
#define TAS_PTHREAD_COMPAT_H

#include "tas.h"

// Replaces Ticket lock by mutex lock
#if TAS_POSIX == 1
#define ticket_lock(foo) pthread_mutex_lock(foo)
#define ticket_unlock(foo) pthread_mutex_unlock(foo)
#endif

#endif // TAS_PTHREAD_COMPAT_H
