#ifndef __CAMERA_COMMUNICATION_H_
#define __CAMERA_COMMUNICATION_H_

#include <trusty_std.h>

#define CAMERA_PORT 3
#define DEBUG_PORT 1

bool receive_message_from_camera(char *received_msg_from_cam, int *size_of_message);
bool send_command_to_camera(char *received_msg_from_cam, int *size_of_message, int command);
int format_recieved_data(char *received_msg_from_cam, int *size_of_message, char *formatted_message);

#endif