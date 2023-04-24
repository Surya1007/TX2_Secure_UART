/*
 * Copyright (c) 2020, NVIDIA Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "tipc.h"

#define TIPC_DEFAULT_NODE "/dev/trusty-ipc-dev0"
#define TA_SUART_NAME "uart.secure.service"

typedef struct secure_camera_msg{
	uint8_t command_type;
	uint32_t io_data;
}secure_camera_msg_t;

int main(int argc, char *argv[]) {
	int secure_comm_fd = tipc_connect(TIPC_DEFAULT_NODE, TA_SUART_NAME);
	if (secure_comm_fd < 0) {
		LOG("SECURE UART: tipc connect fail.\n");
		return -1;
	}
	secure_camera_msg_t msg = {0, 1};
	uint32_t msg_size = sizeof(secure_camera_msg_t);
	write(secure_comm_fd, msg, msg_size);
	read(secure_comm_fd, msg, msg_size);
	return 0;
}
