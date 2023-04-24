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
#include <common.h>
#include <trusty_ipc_setup.h>
#include <err.h>
#include <stdio.h>
#include <trusty_std.h>
#include <platform/secure_port.h>

// ---------------------------------------------------------------------------------------------------------------
//									TA main function for running secure camera			 						//
// ---------------------------------------------------------------------------------------------------------------
int main(void)
{
	int rc;
	uevent_t IPC_event;

	// Initialize ports
	rc = init_secure_port();
	if (rc < 0)
	{
		TLOGI("Failed (%d) to init IPC service", rc);
		kill_secure_port();
		return -1;
	}
	handle_t port = (handle_t)rc;
	TLOGI("Initialization completed\n");
	// Handle IPC service events
	do
	{
		IPC_event.handle = INVALID_IPC_HANDLE;
		IPC_event.event = 0;
		IPC_event.cookie = NULL;

		rc = wait(port, &IPC_event, -1);
		if (rc < 0)
		{
			TLOGI("wait_any failed (%d)", rc);
			continue;
		}

		if (rc == NO_ERROR)
		{
			// Got a port event
			handle_port_event(&IPC_event);
			if (rc < 0)
			{
				TLOGE("Exiting TA\n");
				return -1;
			}
		}
		TLOGI("Poooora\n");
	} while (1);

	TLOGI("Got here\n");
	return 0;
}
