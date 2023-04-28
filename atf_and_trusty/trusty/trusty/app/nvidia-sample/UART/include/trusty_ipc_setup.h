#ifndef __TRUSTY_IPC_SETUP_H_
#define __TRUSTY_IPC_SETUP_H_

#include <trusty_std.h>

typedef struct secure_camera_msg
{
	int camera_command;
    int additional_args_received_msg_length;
	char camera_response[1018*4];
} secure_camera_msg_t;



int init_secure_port( void );
void kill_secure_port( void );
void handle_port_event(const uevent_t * ev);

#endif