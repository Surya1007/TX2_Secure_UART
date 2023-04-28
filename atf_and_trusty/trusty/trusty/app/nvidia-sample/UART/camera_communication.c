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

        // NOTE: FOR READING THE IMAGE DATA LENGTH, NEED TO ADD OTHER STUFF AS WELL
        // 56 00 34 01 II
        // THIS VERSION READS THE CURRENT IMAGE DATA LENGTH ONLY
        "5600340100", // Read current image data length

        // NOTE: FOR READ IMAGE DATA, NEED TO ADD STUFF AS WELL. CHECK CAMERA DOCUMENT
        // 56 00 32 0CII 0A SS SS SS SS LL LL LL LL 00 FF
        "5600320C000A", // Read current Image data

        "5600360103", // Stop capture

        // NOTE: FOR SETTING COMPRESSION RATIO, NEED TO ADD STUFF AS WELL. CHECK CAMERA DOCUMENT
        // 56 00 31 05 01 01 12 04 XX
        "56003105010112", // Setting image compression ratio

        // NOTE: FOR SETTING IMAGE RESOLUTION, NEED TO ADD STUFF AS WELL. CHECK CAMERA DOCUMENT
        "560031050401001911", // Setting image resolution
};

bool receive_message_from_camera(char *received_msg_from_cam, int *size_of_message)
{
    bool found = 0;
    int size = 0;
    char *recieved_byte = (char *)malloc(sizeof(char));
    while (receive_from_uart(CAMERA_PORT, recieved_byte, 1) == 0)
    {
        found = 1;
        strcat(received_msg_from_cam, recieved_byte);
        size++;
    }
    *size_of_message = size;
    free(recieved_byte);
    return found;
}

bool send_command_to_camera(char *received_msg_from_cam, int *size_of_message, int command)
{
    bool found = false;
    send_to_uart(DEBUG_PORT, (char *)"Sent command: ", 1);
    /*
        switch (command)
        {
        // Get version number
        case 0:
        {
            send_to_uart(DEBUG_PORT, (char *)commands[0], 1);
            send_to_uart(DEBUG_PORT, (char *)"\r\n", 1);
            send_to_uart(CAMERA_PORT, (char *)commands[0], 0);
            found = receive_message_from_camera(received_msg_from_cam, size_of_message);
            break;
        }
        // Reset camera
        case 1:
        {
            send_to_uart(DEBUG_PORT, (char *)commands[1], 1);
            send_to_uart(DEBUG_PORT, (char *)"\r\n", 1);
            send_to_uart(CAMERA_PORT, (char *)commands[1], 0);
            found = receive_message_from_camera(received_msg_from_cam, size_of_message);
            break;
        }
        // Capture image
        case 2:
        {
            send_to_uart(DEBUG_PORT, (char *)commands[2], 1);
            send_to_uart(DEBUG_PORT, (char *)"\r\n", 1);
            // Capture an image
            send_to_uart(CAMERA_PORT, (char *)commands[2], 0);
            found = receive_message_from_camera(received_msg_from_cam, size_of_message);
            if (found == 1)
            {
                send_to_uart(DEBUG_PORT, (char *)"Received capture response\r\n", 1);
                if (strcmp(received_msg_from_cam, "\x76\x00\x36\x00"))
                {
                    send_to_uart(DEBUG_PORT, (char *)"Captured an image\r\n", 1);
                }
            }
            send_to_uart(DEBUG_PORT, (char *)"Done with receiving image from camera\r\n", 1);
            break;
        }
        // Invalid command given to camera
        default:
        {
            send_to_uart(DEBUG_PORT, (char *)"Received invalid command from CA\r\n", 1);
            return false;
        }
        }
        */
    send_to_uart(DEBUG_PORT, (char *)commands[0], 1);
    send_to_uart(DEBUG_PORT, (char *)"\r\n", 1);
    send_to_uart(CAMERA_PORT, (char *)commands[0], 0);
    found = receive_message_from_camera(received_msg_from_cam, size_of_message);
    return found;
}

int format_recieved_data(char *received_msg_from_cam, int *size_of_message, char *formatted_message)
{
    char *to_print = (char *)malloc((*size_of_message) * 3 * sizeof(char));
    char *value_in_char_reverse = (char *)malloc(100 * sizeof(char));
    char *value_in_char = (char *)malloc(100 * sizeof(char));
    int global_size = 0;
    for (int count = 0; count < *size_of_message; count++)
    {
        uint8_t value = (uint8_t)received_msg_from_cam[count];
        int size = 0;
        if (value == 0)
        {
            char temp = '0';
            strncat(value_in_char_reverse, &temp, 1);
            strncat(value_in_char_reverse, &temp, 1);
            size += 2;
            global_size += 2;
        }
        else
        {
            while (value != 0)
            {
                uint8_t rem = value % 10;
                char part_of_val = rem + '0';
                strncat(value_in_char_reverse, &part_of_val, 1);
                value = value / 10;
                size++;
                global_size++;
            }
        }

        for (int i = size - 1; i >= 0; i--)
        {
            strncat(value_in_char, &value_in_char_reverse[i], 1);
        }
        char *space = (char *)" ";
        global_size++;
        strcat(value_in_char, space);
        strcat(to_print, value_in_char);
        memset(value_in_char, 0, 100);
        memset(value_in_char_reverse, 0, 100);
    }
    send_to_uart(DEBUG_PORT, to_print, 1);
    strcpy(formatted_message, to_print);
    send_to_uart(DEBUG_PORT, (char *)"\r\nDone sending\r\n", 1);
    // free(to_print);
    free(value_in_char_reverse);
    free(value_in_char);
    return global_size;
}