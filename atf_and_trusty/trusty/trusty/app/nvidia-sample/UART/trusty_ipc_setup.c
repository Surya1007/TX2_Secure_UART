#include <common.h>
#include <trusty_ipc_setup.h>
#include <trusty_std.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <camera_communication.h>

typedef struct port_info
{
	const char *port_name;
	handle_t port_handle;
	uint32_t num_of_buffers;
	size_t ind_buffer_size;
	uint32_t port_flags;
} port_info_t;

static secure_camera_msg_t msg_buf[1];
static char *message_from_camera;

static struct port_info ports[] =
	{
		{
			.port_name = "uart.secure.service",
			.port_handle = NULL,
			.num_of_buffers = 1,
			.ind_buffer_size = 1024 * 4,
			.port_flags = IPC_PORT_ALLOW_NS_CONNECT | IPC_PORT_ALLOW_TA_CONNECT,
		},
};

const char *failure_outputs[2] =
	{
		"Failed to get version number\r\n",
		"Failed to get response from camera\r\n",
};

// ---------------------------------------------------------------------------------------------------------------
//						Function to initialize trusty port for secure UART communication			 			//
// ---------------------------------------------------------------------------------------------------------------
int init_secure_port()
{
	TLOGI("Initializing trusty port for secure-uart communication\n");
	int rc;
	rc = port_create(ports[0].port_name, ports[0].num_of_buffers, ports[0].ind_buffer_size, ports[0].port_flags);
	ports[0].port_handle = (handle_t)rc;
	if (rc < 0)
	{
		TLOGI("Failed (%d) to create port\n", rc);
		return rc;
	}
	return rc;
}

// ---------------------------------------------------------------------------------------------------------------
//						Function to de-initialize trusty port for secure UART communication			 			//
// ---------------------------------------------------------------------------------------------------------------
void kill_secure_port()
{
	TLOGI("Killing secure port\n");
	if (ports[0].port_handle != INVALID_IPC_HANDLE)
	{
		int rc = close((uint32_t)ports[0].port_handle);
		if (rc != NO_ERROR)
		{
			TLOGI("Failed (%d) to close port: %d", rc, ports[0].port_handle);
		}
		ports[0].port_handle = INVALID_IPC_HANDLE;
	}
}

// ---------------------------------------------------------------------------------------------------------------
//						Function to retrieve and send message for secure UART communication			 			//
// ---------------------------------------------------------------------------------------------------------------
static int handle_msg(handle_t chan)
{
	int rc;
	iovec_t iov;
	ipc_msg_t msg;
	ipc_msg_info_t msg_inf;

	iov.base = msg_buf;
	iov.len = sizeof(msg_buf);

	msg.num_iov = ports[0].num_of_buffers;
	msg.iov = &iov;
	msg.num_handles = 0;
	msg.handles = NULL;

	// get message info
	rc = get_msg(chan, &msg_inf);
	if (rc == ERR_NO_MSG)
		return NO_ERROR; // no new messages

	if (rc != NO_ERROR)
	{
		TLOGE("failed (%d) to get_msg for chan (%d)\n", rc, chan);
		return rc;
	}

	// read msg content
	rc = read_msg(chan, msg_inf.id, 0, &msg);
	if (rc < 0)
	{
		TLOGE("failed (%d) to read_msg for chan (%d)\n", rc, chan);
		return rc;
	}

	secure_camera_msg_t *temporary_storage_msg = ((secure_camera_msg_t *)msg.iov[0].base);

	// Read the command received from CA
	int received_command = temporary_storage_msg->camera_command;
	char *image_data = (char *)malloc(1024 * sizeof(char));

	// // Debugging purpose--------------------------------------------------------
	// char *tempo = (char *)"Received command: ";
	// send_to_uart(DEBUG_PORT, tempo, 1);
	// char *data_to_send = (char *)malloc(100 * sizeof(char));
	// int co = 0;
	// for (int i = received_command; i > 0; i /= 10, co++)
	// {
	// 	data_to_send[co] = (i % 10) + '0';
	// }
	// if (received_command == 0)
	// {
	// 	data_to_send[0] = '0';
	// }
	// send_to_uart(DEBUG_PORT, data_to_send, 1);
	// tempo = (char *)" from CA\r\n";
	// send_to_uart(DEBUG_PORT, tempo, 1);
	// free(data_to_send);
	// // Debugging purpose Ends here-----------------------------------------------

	// update number of bytes received from CA
	iov.len = (size_t)rc;
	int received_size;

	// **************************************************************************
	// 					Receive version number from camera
	//									OR
	//					Flush values from the camera tx buffer
	// **************************************************************************

	message_from_camera = (char *)malloc(500 * sizeof(char));
	bool found = receive_message_from_camera(message_from_camera, &received_size);
	if (found == 1)
	{
		char *temp = (char *)"Received version number\r\n";
		send_to_uart(DEBUG_PORT, temp, 1);
	}
	else
	{
		send_to_uart(DEBUG_PORT, (char *)failure_outputs[0], 1);
	}
	memset(message_from_camera, '\0', 500 * sizeof(char));

	// **************************************************************************
	// 					Set Image resolution
	// **************************************************************************

	bool initialize_status = send_command_to_camera(message_from_camera,
													&received_size, received_command);
	if (initialize_status == 0)
	{
		send_to_uart(DEBUG_PORT, (char *)failure_outputs[1], 1);
	}
	else
	{
		char *temp = (char *)"Received the cameraaaaa response:\n\n\n";
		send_to_uart(DEBUG_PORT, temp, 1);
		send_to_uart(DEBUG_PORT, message_from_camera, 1);
		// int frmt_msg_size = format_recieved_data(message_from_camera, &received_size, image_data);
		//  frmt_msg_size += 1;
		temporary_storage_msg->additional_args_received_msg_length = received_size;
		// char *rewq = (char *)"Hello from secure world\n";
		for (unsigned int i = 0; i < strlen(message_from_camera); i++)
		{
			temporary_storage_msg->camera_response[i] = (char)message_from_camera[i];
		}
		temp = (char *)"Received the camera response:\r\n";
		send_to_uart(DEBUG_PORT, temp, 1);
		send_to_uart(DEBUG_PORT, temporary_storage_msg->camera_response, 1);
		send_to_uart(DEBUG_PORT, (char *)"\n\n\n\r", 1);
	}

	//**********************************************************************************

	send_to_uart(DEBUG_PORT, (char *)"\r\nCompleted\n\n\r", 1);
	free(image_data);
	free(message_from_camera);

	/* send message back to the caller */
	rc = send_msg(chan, &msg);
	if (rc < 0)
	{
		TLOGE("failed (%d) to send_msg for chan (%d)\n", rc, chan);
		return rc;
	}

	rc = send_msg(chan, &msg);
	if (rc < 0)
	{
		TLOGE("failed (%d) to send_msg for chan (%d)\n", rc, chan);
		return rc;
	}

	/* retire message */
	rc = put_msg(chan, msg_inf.id);
	if (rc != NO_ERROR)
	{
		TLOGE("failed (%d) to put_msg for chan (%d)\n", rc, chan);
		return rc;
	}

	return NO_ERROR;
}

// ---------------------------------------------------------------------------------------------------------------
//						Function to handle a channel event 			 				 							//
// ---------------------------------------------------------------------------------------------------------------
static void handle_channel_event(const uevent_t *ev)
{
	int rc;

	if (ev->event & IPC_HANDLE_POLL_MSG)
	{
		rc = handle_msg(ev->handle);
		if (rc != NO_ERROR)
		{
			/* report an error and close channel */
			TLOGE("failed (%d) to handle event on channel %d\n", rc, ev->handle);
			close(ev->handle);
		}
		return;
	}
	if (ev->event & IPC_HANDLE_POLL_HUP)
	{
		/* closed by peer. */
		close(ev->handle);
		return;
	}
}

// ---------------------------------------------------------------------------------------------------------------
//						Function to handle a port event														 	//
// ---------------------------------------------------------------------------------------------------------------
void handle_port_event(const uevent_t *ev)
{
	uuid_t peer_uuid;

	if ((ev->event & IPC_HANDLE_POLL_ERROR) ||
		(ev->event & IPC_HANDLE_POLL_HUP) ||
		(ev->event & IPC_HANDLE_POLL_MSG) ||
		(ev->event & IPC_HANDLE_POLL_SEND_UNBLOCKED))
	{
		/* should never happen with port handles */
		TLOGE("error event (0x%x) for port (%d)\n", ev->event, ev->handle);
		// abort();
		return;
	}
	if (ev->event & IPC_HANDLE_POLL_READY)
	{
		/* incoming connection: accept it */
		int rc = accept(ev->handle, &peer_uuid);
		if (rc < 0)
		{
			TLOGE("failed (%d) to accept on port %d\n", rc, ev->handle);
			return;
		}
		handle_t chan = rc;
		while (true)
		{
			struct uevent cev;
			rc = wait(chan, &cev, INFINITE_TIME);
			if (rc < 0)
			{
				TLOGE("wait returned (%d)\n", rc);
				// abort();
				return;
			}
			handle_channel_event(&cev);
			if (cev.event & IPC_HANDLE_POLL_HUP)
			{
				return;
			}
		}
	}
}
