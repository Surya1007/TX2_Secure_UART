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

typedef struct secure_camera_msg
{
	int camera_command;
	int additional_args;
	char *camera_data;
	char *camera_response;
} secure_camera_msg_t;

int main(int argc, char *argv[])
{
	if (argc < 1)
	{
		printf("Please enter the camera command:\n");
		printf("1. Get camera version\n");
		printf("2. Reset camera\n");
		printf("3. Take snapshot\n");
		printf("4. Pass the following arguments as well to change compression ratio to size:\n");
		printf("\t1. High Image quality\n");
		printf("\t2. Good Image quality\n");
		printf("\t3. Low Image quality\n");
		printf("5. Pass the following arguments as well to change the image resolution to size:\n");
		printf("\t1. 11.2 kB\n");
		printf("\t2. 36 kB\n");
		printf("\t3. 45 kB\n");
		printf("\t4. 80 kB\n");
		printf("\t5. 92 kB\n");
		printf("\t6. 136 kB\n");
		printf("\t7. 520 kB\n");
		return -1;
	}
	if ((strtol(argv[0], NULL, 10) == 4) || (strtol(argv[0], NULL, 10) == 5))
	{
		if (argc != 2)
		{
			printf("Please enter the image resolution/compression ratio\n");
			return -1;
		}
	}
	if (strtol(argv[0], NULL, 10) > 5)
	{
		printf("Please enter a valid command\n");
		return -1;
	}

	secure_camera_msg_t msg = {strtol(argv[0], NULL, 10), strtol(argv[1], NULL, 10), NULL, NULL};
	msg.camera_response = (char *)malloc(1024 * sizeof(char));
	if (strtol(argv[0], NULL, 10) == 3)
	{
		msg.camera_data = (char *)malloc(520 * 1024 * sizeof(char));
	}
	uint32_t msg_size = sizeof(secure_camera_msg_t);

	// **********************************************************************
	// 						TRUSTY COMMUNICATION STARTS HERE
	// **********************************************************************
	int secure_comm_fd = tipc_connect(TIPC_DEFAULT_NODE, TA_SUART_NAME);
	if (secure_comm_fd < 0)
	{
		LOG("SECURE UART: tipc connect fail.\n");
		return -1;
	}
	write(secure_comm_fd, &msg, msg_size);
	read(secure_comm_fd, &msg, msg_size);
	// **********************************************************************
	// 						TRUSTY COMMUNICATION ENDS HERE
	// **********************************************************************

	free(msg.camera_response);
	free(msg.camera_data);
	return 0;
}
