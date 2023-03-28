#ifndef __PLATFORM_SECURE_H
#define __PLATFORM_SECURE_H

#include <lib/trusty/uuid.h>
#include <stdbool.h>
#include <lib/sm.h>
#if defined(WITH_PLATFORM_PARTNER)
#include <partner/platform/platform_p.h>
#endif



void platform_init_secure_port(unsigned int dbg_port);
void platform_disable_secure_intf(void);
void platform_enable_secure_intf(void);



#endif
