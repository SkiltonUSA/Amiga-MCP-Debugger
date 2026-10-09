#ifndef __ASSEMBLER__
/* Override picolibc's configured TLS errno: this is one bare-metal worker. */
#include <sys/cdefs.h>
#undef __GLOBAL_ERRNO
#define __GLOBAL_ERRNO 1

#endif
