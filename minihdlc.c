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
#define low(x)    ((x) & 0xFF)
#define high(x)   (((x)>>8) & 0xFF)

#include <stdint.h>

#define lo8(x) ((x)&0xff) 
#define hi8(x) ((x)>>8)

static uint16_t crc16_update(uint16_t crc, uint8_t a) {
	int i;

	crc ^= a;
	for (i = 0; i < 8; ++i) {
		if (crc & 1)
			crc = (crc >> 1) ^ 0xA001;
		else
			crc = (crc >> 1);
	}

	return crc;
}

static uint16_t crc_xmodem_update(uint16_t crc, uint8_t data) {
	int i;

	crc = crc ^ ((uint16_t) data << 8);
	for (i = 0; i < 8; i++) {
		if (crc & 0x8000)
			crc = (crc << 1) ^ 0x1021;
		else
			crc <<= 1;
	}

	return crc;
}
static uint16_t _crc_ccitt_update(uint16_t crc, uint8_t data) {
	data ^= lo8(crc);
	data ^= data << 4;

	return ((((uint16_t) data << 8) | hi8(crc)) ^ (uint8_t) (data >> 4)
			^ ((uint16_t) data << 3));
}

static uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data) {
	uint8_t i;

	crc = crc ^ data;
	for (i = 0; i < 8; i++) {
		if (crc & 0x01)
			crc = (crc >> 1) ^ 0x8C;
		else
			crc >>= 1;
	}

	return crc;
}

struct {
	sendchar_type sendchar_function;
	frame_handler_type frame_handler;
	bool escape_character;
	uint16_t frame_position;
	uint16_t frame_checksum;
	uint8_t receive_frame_buffer[MINIHDLC_MAX_FRAME_LENGTH + 1];
} mhst;

void minihdlc_init(sendchar_type put_char,
		frame_handler_type hdlc_command_router) {
	mhst.sendchar_function = put_char;
	mhst.frame_handler = hdlc_command_router;
	mhst.frame_position = 0;
	mhst.frame_checksum = CRC16_CCITT_INIT_VAL;
	mhst.escape_character = false;
}

/* Function to send a byte throug USART, I2C, SPI etc.*/
static void minihdlc_sendchar(uint8_t data) {
	(*mhst.sendchar_function)(data);
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

	if (mhst.frame_position - 2 >= 0) {
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
void minihdlc_send_frame(const uint8_t *framebuffer, uint8_t frame_length) {
	uint8_t data;
	uint16_t fcs = CRC16_CCITT_INIT_VAL;

	minihdlc_sendchar((uint8_t) FRAME_BOUNDARY_OCTET);

	while (frame_length) {
		data = *framebuffer++;
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

