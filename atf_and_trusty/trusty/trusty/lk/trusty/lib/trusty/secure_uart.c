#include <assert.h>
#include <debug.h>
#include <err.h>
#include <kernel/thread.h>
#include <kernel/mutex.h>
#include <mm.h>
#include <stdlib.h>
#include <string.h>
#include <trace.h>
#include <uthread.h>

#include <lib/trusty/sys_fd.h>
#include <lib/trusty/trusty_app.h>
#include <trusty_std.h>
#include <platform/secure_port.h>

#define LOCAL_TRACE 0





long sys_send_to_uart(uint32_t uart_port, char* message, uint32_t msg_size);
long sys_receive_from_uart (uint32_t uart_port, char* message, uint32_t wait);


long sys_send_to_uart(uint32_t uart_port, char* message, uint32_t msg_size)
{
	for( uint32_t i = 0; i < msg_size; i++ )
	{
		secure_platform_dputc(message[i]);
	}
	return 0;
}

long sys_receive_from_uart (uint32_t uart_port, char* message, uint32_t wait)
{
	int status = secure_platform_dgetc( message,  (bool) wait);
	return status;
}
