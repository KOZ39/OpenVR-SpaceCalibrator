#include "platform.h"

namespace spacecal {
#if OS_WINDOWS
void init_crash_handler();
void shutdown_crash_handler();
#else
// no-op on linux
inline void init_crash_handler() { }
inline void shutdown_crash_handler() { }
#endif
} // spacecal