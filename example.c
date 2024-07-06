// compile: gcc -o example example.c .\minihdlc.c -DMINIHDLC_MAX_FRAME_LENGTH=1024

#include <stdio.h>
#include <string.h>
#include "minihdlc.h"

void send_char(uint8_t data)
{
    printf("Received char(hex): %02x\n", data);
    minihdlc_char_receiver(data);
}

void frame_received(const uint8_t *frame_buffer, uint16_t frame_length)
{
    printf("Received frame: ");
    for (uint16_t i = 0; i < frame_length; i++)
    {
        putc(frame_buffer[i], stdout);
    }
}

int main(int argc, const char **argv)
{
    minihdlc_init(send_char, frame_received);
    char string_buffer[256];
    for (int i = 0; i < 10; i++)
    {
        snprintf(string_buffer, sizeof(string_buffer), "i = %d\n", i);
        minihdlc_send_frame((const uint8_t *)string_buffer, strlen(string_buffer));
    }
}