#pragma once

/* CMake selects one threading ABI for the library and its consumers. */
#ifdef CLM_USE_TINYCTHREAD
#include <tinycthread.h>
#else
#include <threads.h>
#endif
