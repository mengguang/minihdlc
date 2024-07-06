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

// init the internal data structure and
// set the user defined callback function
void minihdlc_init(sendchar_type sendchar_function,
				   frame_handler_type frame_hander_function);

// on the receiver side, put data into internal buffer,
// When a complete frame is received,
// the frame_received_handler_function will be called.
void minihdlc_char_receiver(uint8_t data);

// on the sender side,
// encode frame and call send_char_function to send the data.
void minihdlc_send_frame(const uint8_t *frame_buffer, uint8_t frame_length);

#endif
