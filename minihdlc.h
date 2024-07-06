#ifndef minihdlc_h
#define minihdlc_h

#include <stdint.h>
#include <stdbool.h>

typedef void (*sendchar_type)(uint8_t data);
typedef void (*frame_handler_type)(const uint8_t *frame_buffer,
		uint16_t frame_length);

#ifndef MINIHDLC_MAX_FRAME_LENGTH
#define MINIHDLC_MAX_FRAME_LENGTH (64)
#endif

void minihdlc_init(sendchar_type sendchar_function,
		frame_handler_type frame_hander_function);
void minihdlc_char_receiver(uint8_t data);
void minihdlc_send_frame(const uint8_t *frame_buffer, uint8_t frame_length);

#endif
