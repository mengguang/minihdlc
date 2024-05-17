#include "minihdlc.h"

/* HDLC Asynchronous framing */
/* The frame boundary octet is 01111110, (7E in hexadecimal notation) */
#define FRAME_BOUNDARY_OCTET 0x7E

/* A "control escape octet", has the bit sequence '01111101', (7D hexadecimal) */
#define CONTROL_ESCAPE_OCTET 0x7D

/* If either of these two octets appears in the transmitted data, an escape octet is sent, */
/* followed by the original data octet with bit 5 inverted */
#define INVERT_OCTET 0x20

/* The frame check sequence (FCS) is a 16-bit CRC-CCITT */
/* AVR Libc CRC function is _crc_ccitt_update() */
/* Corresponding CRC function in Qt (www.qt.io) is qChecksum() */
#define CRC16_CCITT_INIT_VAL 0xFFFF

/* 16bit low and high bytes copier */
#define low(x)		((x) & 0xFF)
#define high(x)		(((x)>>8) & 0xFF)

#define lo8(x)		((x)&0xff)
#define hi8(x)		((x)>>8)

struct {
	sendchar_type sendchar_function;
	frame_handler_type frame_handler;
	bool escape_character;
	uint16_t frame_position;
	uint16_t frame_checksum;
	uint8_t receive_frame_buffer[MINIHDLC_MAX_FRAME_LENGTH + 1];
} mhst;

/*
 Polynomial: x^16 + x^12 + x^5 + 1 (0x8408) Initial value: 0xffff
 This is the CRC used by PPP and IrDA.
 See RFC1171 (PPP protocol) and IrDA IrLAP 1.1
 */
static uint16_t _crc_ccitt_update(uint16_t crc, uint8_t data) {
	data ^= lo8(crc);
	data ^= data << 4;

	return ((((uint16_t) data << 8) | hi8(crc)) ^ (uint8_t) (data >> 4)
			^ ((uint16_t) data << 3));
}

void minihdlc_init(sendchar_type sendchar_function,
		frame_handler_type frame_hander_function) {
	mhst.sendchar_function = sendchar_function;
	mhst.frame_handler = frame_hander_function;
	mhst.frame_position = 0;
	mhst.frame_checksum = CRC16_CCITT_INIT_VAL;
	mhst.escape_character = false;
}

/* Function to send a byte throug USART, I2C, SPI etc.*/
static inline void minihdlc_sendchar(uint8_t data) {
	if (mhst.sendchar_function) {
		(*mhst.sendchar_function)(data);
	}
}

/* Function to find valid HDLC frame from incoming data */
void minihdlc_char_receiver(uint8_t data) {
	/* FRAME FLAG */
	if (data == FRAME_BOUNDARY_OCTET) {
		if (mhst.escape_character == true) {
			mhst.escape_character = false;
		}
		/* If a valid frame is detected */
		else if ((mhst.frame_position >= 2)
				&& (mhst.frame_checksum
						== ((mhst.receive_frame_buffer[mhst.frame_position - 1]
								<< 8)
								| (mhst.receive_frame_buffer[mhst.frame_position
										- 2] & 0xff)))) // (msb << 8 ) | (lsb & 0xff)
				{
			/* Call the user defined function and pass frame to it */
			(*mhst.frame_handler)(mhst.receive_frame_buffer,
					mhst.frame_position - 2);
		}
		mhst.frame_position = 0;
		mhst.frame_checksum = CRC16_CCITT_INIT_VAL;
		return;
	}

	if (mhst.escape_character) {
		mhst.escape_character = false;
		data ^= INVERT_OCTET;
	} else if (data == CONTROL_ESCAPE_OCTET) {
		mhst.escape_character = true;
		return;
	}

	mhst.receive_frame_buffer[mhst.frame_position] = data;

	if (mhst.frame_position >= 2) {
		mhst.frame_checksum = _crc_ccitt_update(mhst.frame_checksum,
				mhst.receive_frame_buffer[mhst.frame_position - 2]);
	}

	mhst.frame_position++;

	if (mhst.frame_position == MINIHDLC_MAX_FRAME_LENGTH) {
		mhst.frame_position = 0;
		mhst.frame_checksum = CRC16_CCITT_INIT_VAL;
	}
}

/* Wrap given data in HDLC frame and send it out byte at a time*/
void minihdlc_send_frame(const uint8_t *frame_buffer, uint8_t frame_length) {
	uint8_t data;
	uint16_t fcs = CRC16_CCITT_INIT_VAL;

	minihdlc_sendchar((uint8_t) FRAME_BOUNDARY_OCTET);

	while (frame_length) {
		data = *frame_buffer++;
		fcs = _crc_ccitt_update(fcs, data);
		if ((data == CONTROL_ESCAPE_OCTET) || (data == FRAME_BOUNDARY_OCTET)) {
			minihdlc_sendchar((uint8_t) CONTROL_ESCAPE_OCTET);
			data ^= INVERT_OCTET;
		}
		minihdlc_sendchar((uint8_t) data);
		frame_length--;
	}
	data = low(fcs);
	if ((data == CONTROL_ESCAPE_OCTET) || (data == FRAME_BOUNDARY_OCTET)) {
		minihdlc_sendchar((uint8_t) CONTROL_ESCAPE_OCTET);
		data ^= (uint8_t) INVERT_OCTET;
	}
	minihdlc_sendchar((uint8_t) data);
	data = high(fcs);
	if ((data == CONTROL_ESCAPE_OCTET) || (data == FRAME_BOUNDARY_OCTET)) {
		minihdlc_sendchar(CONTROL_ESCAPE_OCTET);
		data ^= INVERT_OCTET;
	}
	minihdlc_sendchar(data);
	minihdlc_sendchar(FRAME_BOUNDARY_OCTET);
}

/* Wrap given data in HDLC frame and send it to static buffer*/

static uint8_t frame_buffer[MINIHDLC_MAX_FRAME_LENGTH + 1];
static uint32_t frame_buffer_size = 0;

static void buffer_init() {
	frame_buffer_size = 0;
}

static void buffer_push(uint8_t data) {
	if (frame_buffer_size >= MINIHDLC_MAX_FRAME_LENGTH) {
		return;
	}
	frame_buffer[frame_buffer_size] = data;
	frame_buffer_size++;
}

void minihdlc_send_frame_to_buffer(const uint8_t *frame_buffer,
		uint8_t frame_length) {
	mhst.sendchar_function = buffer_push;
	buffer_init();
	minihdlc_send_frame(frame_buffer, frame_length);
}

uint8_t *minihdlc_get_buffer() {
	return frame_buffer;
}

uint32_t minihdlc_get_buffer_size() {
	return frame_buffer_size;
}
