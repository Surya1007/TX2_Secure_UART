#ifndef __PLATFORM_SECURE_H
#define __PLATFORM_SECURE_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdarg.h>
#include <compiler.h>

__BEGIN_CDECLS

void secure_platform_dputc(char c);
int secure_platform_dgetc(char *c, bool wait);

__END_CDECLS


#endif
