#ifndef __TRUSTY_IPC_SETUP_H_
#define __TRUSTY_IPC_SETUP_H_

#include <trusty_std.h>

typedef struct secure_camera_msg
{
	int camera_command;
    int additional_args;
	char *camera_data;
	char camera_response[500];
} secure_camera_msg_t;

int init_secure_port( void );
void kill_secure_port( void );
void handle_port_event(const uevent_t * ev);

#endif