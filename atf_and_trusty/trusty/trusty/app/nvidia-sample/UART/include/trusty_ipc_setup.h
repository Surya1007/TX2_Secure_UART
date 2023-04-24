#ifndef __TRUSTY_IPC_SETUP_H_
#define __TRUSTY_IPC_SETUP_H_

#include <trusty_std.h>

int init_secure_port( void );
void kill_secure_port( void );
void handle_port_event(const uevent_t * ev);

#endif