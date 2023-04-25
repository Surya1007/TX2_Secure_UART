#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <trusty_std.h>
#include <camera_communication.h>
#include <common.h>
#include <err.h>

static const char *commands[8] =
    {
        "56001100",   // Get Version
        "56002600",   // Reset
        "5600360100", // Capture an image
        "5600340100", // Read current image data length
        // NOTE: FOR READ IMAGE DATA, NEED TO ADD STUFF AS WELL. CHECK CAMERA DOCUMENT
        // 56 00 32 0CII 0A SS SS SS SS LL LL LL LL 00 FF
        "5600320C000A", // Read Image data
        "5600360103",   // Stop capture
        // NOTE: FOR SETTING COMPRESSION RATIO, NEED TO ADD STUFF AS WELL. CHECK CAMERA DOCUMENT
        // 56 00 31 05 01 01 12 04 XX
        "5600310501011204", // Setting image compression ratio
        // NOTE: FOR SETTING IMAGE RESOLUTION, NEED TO ADD STUFF AS WELL. CHECK CAMERA DOCUMENT
        "560031050401001911", // Setting image resolution
};

bool receive_message_from_camera(char *received_msg_from_cam, int *size_of_message)
{
    bool found = 0;
    int size = 0;
    send_to_uart(DEBUG_PORT, (char *)"Enter\n\r", 1);
    char *recieved_byte = (char *)malloc(sizeof(char));
    while (receive_from_uart(CAMERA_PORT, recieved_byte, 1) == 0)
    {
        found = 1;
        strcat(received_msg_from_cam, recieved_byte);
        size++;
        send_to_uart(DEBUG_PORT, (char *)"go\n\r", 1);
    }

    *size_of_message = size;
    free(recieved_byte);
    send_to_uart(DEBUG_PORT, (char *)"Exit\n\r", 1);
    return found;
}

bool send_command_to_camera(char *received_msg_from_cam, int *size_of_message, int command)
{
    // Send debug outputs
    send_to_uart(DEBUG_PORT, (char *)"Sent command: ", 1);
    send_to_uart(DEBUG_PORT, (char *)commands[command], 1);
    send_to_uart(DEBUG_PORT, (char *)"\n\r", 1);
    send_to_uart(DEBUG_PORT, (char *)commands[command], 0);
    // Send command to camera uart port
    send_to_uart(CAMERA_PORT, (char *)"56001100", 0);
    bool found = receive_message_from_camera(received_msg_from_cam, size_of_message);
    return found;
}

void print_recieved_data(char *received_msg_from_cam, int *size_of_message)
{
    // char *to_print = malloc(1000 * sizeof(char));
    // for (int count = 0; received_msg_from_cam[count] != '\0'; count++)
    // {
    //     char *value_in_char_reverse = malloc(10 * sizeof(char));
    //     int value = (int)((uint8_t)received_msg_from_cam[count]);
    //     TLOGE("Values is %x", value);
    //     int size = 0;
    //     while (value != 0)
    //     {
    //         int rem = value % 10;
    //         char part_of_val = (char)(rem + '0');
    //         strcat(value_in_char_reverse, &part_of_val);
    //         value = value / 10;
    //         size++;
    //     }
    //     char *value_in_char = malloc(size * sizeof(char));
    //     for (int i = size - 1; i > 0; i--)
    //     {
    //         char temp = value_in_char_reverse[i];
    //         strcat(value_in_char, &temp);
    //     }
    //     strcat(to_print, value_in_char);
    //     free(value_in_char_reverse);
    //     free(value_in_char);
    // }
    send_to_uart(DEBUG_PORT, received_msg_from_cam, 1);
    // free(to_print);
}