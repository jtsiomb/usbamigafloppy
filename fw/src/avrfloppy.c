/* USB floppy controller for amiga disks
 *
 * Copyright (C) 2017 Robert Smith (@RobSmithDev)
 * Copyright (C) 2018 John Tsiombikas <nuclear@member.fsf.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, see http://www.gnu.org/licenses
 */
/* Ported from arduino to standalone AVR by John Tsiombikas */

#include <avr/io.h>
#include <avr/interrupt.h>


#define INDEX_PORT			PIND
#define INDEX_BIT			0x04

#define WDATA_PORT			PORTD
#define WDATA_BIT			0x08

#define RDATA_PORT			PIND
#define RDATA_BIT			0x10

#define MOTOR_PORT			PORTD
#define MOTOR_ENABLE_BIT	0x20
#define MOTOR_DIR_BIT		0x40
#define MOTOR_STEP_BIT		0x80

#define TRACK0_PORT			PINB
#define TRACK0_BIT			0x01

#define HEADSEL_PORT		PORTB
#define HEAD_SELECT_BIT		0x02

#define WGATE_PORT			PORTC
#define WGATE_BIT			0x01

#define WPROT_PORT			PINC
#define WPROT_BIT			0x02

#define CTS_PORT			PORTC
#define CTS_BIT				0x04

#define LED_PORT			PORTB
#define LED_BIT				0x20

#define BAUDRATE 2000000

#define BAUD_PRESCALLER_NORMAL_MODE	  (((F_CPU / (BAUDRATE * 16UL))) - 1)
#define BAUD_PRESCALLER_DOUBLESPEED_MODE (((F_CPU / (BAUDRATE * 8UL))) - 1)
/* We're using double speed mode */
#define UART_USE_DOUBLESPEED_MODE

/* Motor directions for PIN settings */
#define MOTOR_TRACK_DECREASE   1
#define MOTOR_TRACK_INCREASE   0
#define MOTOR_DIR(x) ((x) ? (MOTOR_PORT |= MOTOR_DIR_BIT) : (MOTOR_PORT &= ~MOTOR_DIR_BIT))

/* Paula on the Amiga used to find the SYNC WORDS and then read 0x1900 further WORDS.
 * A dos track is 11968 bytes in size, theritical revolution is 12800 bytes.
 * Paula assumed it was 12868 bytes, so we read that, plus thre size of a sectors
 */
#define RAW_TRACKDATA_LENGTH   (0x1900 * 2 + 0x440)

static void setup(void);
static void loop(void);
static void smalldelay(unsigned long delay_time);
static void step_direction_head(void);
static void prep_serial_interface(void);
static inline unsigned char read_byte_from_uart(void);
static inline void write_byte_to_uart(const char value);
static void go_to_track0(void);
static int goto_track_x(void);
static void write_track_from_uart(void);
static void read_track_data_fast(void);

static int current_track; /* The current track that the head is over */
static int drive_enabled; /* If the drive has been switched on or not */
static int in_write_mode; /* If we're in WRITING mode or not */

int main(void)
{
	setup();
	for(;;) {
		loop();
	}
	return 0;
}

static void setup(void)
{
	DDRB = 0x22;	/* outputs: 1 (head sel), 5 (act LED) */
	DDRC = 0xf5;	/* outputs: 0 (wr.gate), 2 (cts) */
	DDRD = 0xe8;	/* outputs: 3 (write), 5 (motor en), 6 (dir), 7 (step) */

	/* Do these right away to prevent the disk being written to */
	PORTC = WGATE_BIT | WPROT_BIT;	/* write gate off and pullup on pin 1 (wprot) */
	PORTD = WDATA_BIT | RDATA_BIT;	/* write data hight and pullup on pin 4 (read) */
	PORTB = TRACK0_BIT;				/* pullup on track0 detect */

	MOTOR_PORT |= MOTOR_ENABLE_BIT;
	HEADSEL_PORT &= ~HEAD_SELECT_BIT;

	/* Disable all interrupts - we dont want them! */
	cli();

	/* Setup the USART */
	prep_serial_interface();
}


/* The main command loop */
static void loop(void)
{
	unsigned char command;

	CTS_PORT &= ~CTS_BIT;		/* Allow data incoming */
	WGATE_PORT |= WGATE_BIT;   /* always turn writing off */

	/* Read the command from the PC */
	command = read_byte_from_uart();

	switch(command) {
	case '?':
		/* Command: "?" Means information about the firmware */
		write_byte_to_uart('1');  /* Success */
		write_byte_to_uart('V');  /* Followed */
		write_byte_to_uart('1');  /* By */
		write_byte_to_uart('.');  /* Version */
		write_byte_to_uart('1');  /* Number */
		break;

		/* Command "." means go back to track 0 */
	case '.':
		if(!drive_enabled) {
			write_byte_to_uart('0');
		} else {
			go_to_track0();	/* reset */
			write_byte_to_uart('1');
		}
		break;

	case '#':
		/* Command "#" means goto track.  Should be formatted as #00 or #32 etc */
		if(!drive_enabled) {
			read_byte_from_uart();
			read_byte_from_uart();
			write_byte_to_uart('0');
		} else {
			if(goto_track_x()) {
				smalldelay(100); /* wait for drive */
				write_byte_to_uart('1');
			} else {
				write_byte_to_uart('0');
			}
		}
		break;

	case '[':
		/* Command "[" select LOWER disk side */
		HEADSEL_PORT &= ~HEAD_SELECT_BIT;
		write_byte_to_uart('1');
		break;

	case ']':
		/* Command "]" select UPPER disk side */
		HEADSEL_PORT |= HEAD_SELECT_BIT;
		write_byte_to_uart('1');
		break;

	case '<':
		/* Command "<" Read track from the drive */
		if(!drive_enabled) {
			write_byte_to_uart('0');
		} else {
			write_byte_to_uart('1');
			read_track_data_fast();
		}
		break;

	case '>':
		/* Command ">" Write track to the drive */
		if(!drive_enabled) {
			write_byte_to_uart('0');
		} else {
			if(!in_write_mode) {
				write_byte_to_uart('0');
			} else {
				write_byte_to_uart('1');
				write_track_from_uart();
			}
		}
		break;

	case '-':
		/* Turn off the drive motor */
		MOTOR_PORT |= MOTOR_ENABLE_BIT;
		WGATE_PORT |= WGATE_BIT;
		drive_enabled = 0;
		write_byte_to_uart('1');
		in_write_mode = 0;
		break;

	case '+':
		/* Turn on the drive motor and setup in READ MODE */
		if(in_write_mode) {
			/* Ensure writing is turned off */
			MOTOR_PORT |= MOTOR_ENABLE_BIT;
			WGATE_PORT |= WGATE_BIT;
			smalldelay(100);
			drive_enabled = 0;
			in_write_mode = 0;
		}
		if(!drive_enabled) {
			MOTOR_PORT &= ~MOTOR_ENABLE_BIT;
			drive_enabled = 1;
			smalldelay(750); /* wait for drive */
		}
		write_byte_to_uart('1');
		break;

	case '~':
		/* Turn on the drive motor and setup in WRITE MODE */
		if(drive_enabled) {
			WGATE_PORT |= WGATE_BIT;
			MOTOR_PORT |= MOTOR_ENABLE_BIT;
			drive_enabled = 0;
			smalldelay(100);
		}
		/* We're writing! */
		WGATE_PORT &= ~WGATE_BIT;
		/* Gate has to be pulled LOW BEFORE we turn the drive on */
		MOTOR_PORT &= ~MOTOR_ENABLE_BIT;
		/* Raise the write gate again */
		WGATE_PORT |= WGATE_BIT;
		smalldelay(750); /* wait for drive */

		/* At this point we can see the status of the write protect flag */
		if((WPROT_PORT & WPROT_BIT) == 0) {
			write_byte_to_uart('0');
			in_write_mode = 0;
			MOTOR_PORT |= MOTOR_ENABLE_BIT;
			/*WGATE_PORT |= WGATE_BIT;*/
		} else  {
			in_write_mode = 1;
			drive_enabled = 1;
			write_byte_to_uart('1');
		}
		break;

	default:
		/* We don't recognise the command! */
		write_byte_to_uart('!'); /* error */
		break;
	}
}


/* Because we turned off interrupts delay() doesnt work! */
static void smalldelay(unsigned long delay_time)
{
	unsigned long i;

	delay_time *= (F_CPU / 9000);

	for(i=0; i<delay_time; ++i) {
		asm volatile("nop");
	}
}

/* Step the head once.  This seems to be an acceptable speed for the head */
static void step_direction_head(void)
{
	smalldelay(5);
	MOTOR_PORT &= ~MOTOR_STEP_BIT;
	smalldelay(5);
	MOTOR_PORT |= MOTOR_STEP_BIT;
}

/* Prepare serial port - We dont want to use the arduino serial library as we
 * want to use faster speeds and no serial interrupts
 */
static void prep_serial_interface(void)
{
#ifdef UART_USE_DOUBLESPEED_MODE
	UBRR0H = (uint8_t)(BAUD_PRESCALLER_DOUBLESPEED_MODE>>8);
	UBRR0L = (uint8_t)(BAUD_PRESCALLER_DOUBLESPEED_MODE);
	UCSR0A |= 1<<U2X0;
#else
	UBRR0H = (uint8_t)(BAUD_PRESCALLER_NORMAL_MODE>>8);
	UBRR0L = (uint8_t)(BAUD_PRESCALLER_NORMAL_MODE);
	UCSR0A &= ~(1<<U2X0);
#endif

	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	/* UsartCharacterSiZe - 8-bit */
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

/* Directly read a byte from the UART0 */
static inline unsigned char read_byte_from_uart(void)
{
	while(!(UCSR0A & (1 << RXC0)));
	return UDR0;
}

/* Directly write a byte to the UART0 */
static inline void write_byte_to_uart(const char value)
{
	while(!(UCSR0A & (1 << UDRE0)));
	UDR0 = value;
}

/* Rewinds the head back to Track 0 */
static void go_to_track0(void)
{
	/* Set the direction to go backwards */
	MOTOR_DIR(MOTOR_TRACK_DECREASE);
	while((TRACK0_PORT & TRACK0_BIT)) {
		step_direction_head();	/* Keep moving the head until we see the TRACK 0 detection pin */
	}
	current_track = 0;	/* Reset the track number */
}

/* Goto a specific track.  During testing it was easier for the track number
 * to be supplied as two ASCII characters, so I left it like this
 */
static int goto_track_x(void)
{
	int track;
	unsigned char track1, track2;

	/* Read the bytes */
	track1 = read_byte_from_uart();
	track2 = read_byte_from_uart();

	/* Validate */
	if((track1 < '0') || (track1 > '9')) return 0;
	if((track2 < '0') || (track2 > '9')) return 0;

	/* Calculate target track and validate */
	track = ((track1 - '0') * 10) + (track2 - '0');
	if(track < 0) return 0;
	if(track > 81) return 0; /* yes amiga could read track 81! */

	/* Exit if its already been reached */
	if(track == current_track) return 1;

	/* And step the head until we reach this track number */
	if(current_track < track) {
		MOTOR_DIR(MOTOR_TRACK_INCREASE);	/* move out */
		while(current_track < track) {
			step_direction_head();
			current_track++;
		}
	} else {
		MOTOR_DIR(MOTOR_TRACK_DECREASE);	/* move in */
		while(current_track > track) {
			step_direction_head();
			current_track--;
		}
	}

	return 1;
}


/* 256 byte circular buffer -
 * don't change this, we abuse the unsigned char to overflow back to zero!
 */
#define SERIAL_BUFFER_SIZE 256
#define SERIAL_BUFFER_START (SERIAL_BUFFER_SIZE - 16)
static unsigned char SERIAL_BUFFER[SERIAL_BUFFER_SIZE];


#define CHECK_SERIAL() \
	do { \
		if(UCSR0A & (1 << RXC0)) { \
			SERIAL_BUFFER[serial_write_pos++] = UDR0; \
			serial_bytes_in_use++; \
		} else if(serial_bytes_in_use < SERIAL_BUFFER_START) { \
			CTS_PORT &= ~CTS_BIT; \
			CTS_PORT |= CTS_BIT; \
		} \
	} while(0)



/* Small Macro to write a '1' pulse to the drive if a bit is set based on the supplied bitmask */
#define WRITE_BIT(value, bitmask) \
	do { \
		if(current_byte & bitmask)  { \
			while(TCNT2 < value); \
			WDATA_PORT &= ~WDATA_BIT; \
		 } else { \
			 while(TCNT2 < value); \
			WDATA_PORT |= WDATA_BIT; \
		} \
	} while(0)

/* Write a track to disk from the UART -
 * the data should be pre-MFM encoded raw track data where '1's are the
 * pulses/phase reversals to trigger
 */
static void write_track_from_uart(void)
{
	unsigned int i, serial_bytes_in_use;
	unsigned char high_byte, low_byte, wait_for_index, current_byte;
	unsigned char serial_read_pos, serial_write_pos;
	unsigned short num_bytes;

	/* Configure timer 2 just as a counter in NORMAL mode */
	TCCR2A = 0;			/* No physical output port pins and normal operation */
	TCCR2B = (1 << CS20);	/* Prescale = 1 */

	/* Check if its write protected.
	 * You can only do this after the write gate has been pulled low
	 */
	if((WPROT_PORT & WPROT_BIT) == 0) {
		write_byte_to_uart('N');
		WGATE_PORT |= WGATE_BIT;
		return;
	}
	write_byte_to_uart('Y');

	/* Find out how many bytes they want to send */
	high_byte = read_byte_from_uart();
	low_byte = read_byte_from_uart();
	wait_for_index = read_byte_from_uart();
	CTS_PORT |= CTS_BIT;	/* stop any more data coming in! */

	num_bytes = (((unsigned short)high_byte) << 8) | low_byte;

	write_byte_to_uart('!');

	/* Signal we're ready for another byte to come */
	CTS_PORT &= ~CTS_BIT;

	/* Fill our buffer to give us a head start */
	for(i=0; i<SERIAL_BUFFER_START; i++) {
		/* Wait for it */
		while(!(UCSR0A & (1 << RXC0)));
		/* Save the byte */
		SERIAL_BUFFER[i] = UDR0;
	}

	/* Stop more bytes coming in, although we expect one more */
	CTS_PORT |= CTS_BIT;

	/* Setup buffer parameters */
	serial_read_pos = 0;
	serial_write_pos = SERIAL_BUFFER_START;
	LED_PORT |= LED_BIT;
	serial_bytes_in_use = SERIAL_BUFFER_START;

	/* Enable writing */
	WGATE_PORT &= ~WGATE_BIT;

	/* While the INDEX pin is high wait.  Might as well write from the start of the track */
	if(wait_for_index) {
		while(INDEX_PORT & INDEX_BIT);
	}

	/* Reset the counter, ready for writing */
	TCNT2 = 0;

	/* Loop them bytes */
	for(i=0; i<num_bytes; i++) {
		/* Should never happen, but we'll wait here if theres no data */
		if(serial_bytes_in_use < 1) {
			/* This can't happen and causes a write failure */
			LED_PORT &= ~LED_BIT;
			/* Thus means buffer underflow. PC wasn't sending us data fast enough */
			write_byte_to_uart('X');
			WGATE_PORT |= WGATE_BIT;
			TCCR2B = 0;   /* No Clock (turn off) */
			return;
		}

		/* Read a buye from the buffer */
		current_byte = SERIAL_BUFFER[serial_read_pos++];
		serial_bytes_in_use--;

		/* Now we write the data. Hopefully by the time we get back to the top
		 * everything is ready again
		 */
		WRITE_BIT(16, 0x80);
		CHECK_SERIAL();
		WRITE_BIT(48, 0x40);
		CHECK_SERIAL();
		WRITE_BIT(80, 0x20);
		CHECK_SERIAL();
		WRITE_BIT(112, 0x10);
		CHECK_SERIAL();
		WRITE_BIT(144, 0x08);
		CHECK_SERIAL();
		WRITE_BIT(176, 0x04);
		CHECK_SERIAL();
		WRITE_BIT(208, 0x02);
		CHECK_SERIAL();
		WRITE_BIT(240, 0x01);
	}
	WGATE_PORT |= WGATE_BIT;

	/* Done! */
	write_byte_to_uart('1');
	LED_PORT &= ~LED_BIT;

	/* Disable the 500khz signal */
	TCCR2B = 0;   /* No Clock (turn off) */
}


/* Read the track using a timings to calculate which MFM sequence has been triggered */
static void read_track_data_fast(void)
{
	unsigned char data_output_byte, counter, bits;
	long total_bits, target;

	/* Configure timer 2 just as a counter in NORMAL mode */
	TCCR2A = 0;			/* No physical output port pins and normal operation */
	TCCR2B = (1 << CS20);	/* Prescale = 1 */

	/* First wait for the serial port to be available */
	while(!(UCSR0A & (1 << UDRE0)));

	/* Signal we're active */
	LED_PORT |= LED_BIT;

	/* While the INDEX pin is high wait if the other end requires us to */
	if(read_byte_from_uart()) {
		while(INDEX_PORT & INDEX_BIT);
	}

	/* Prepare the two counter values as follows: */
	TCNT2=0;	   /* Reset the counter */

	data_output_byte = 0;
	total_bits = 0;
	target = (long)RAW_TRACKDATA_LENGTH * 8L;

	while(total_bits < target) {
		for(bits=0; bits<4; bits++) {
			/* Wait while pin is high */

			while(RDATA_PORT & RDATA_BIT);
			counter = TCNT2;
			TCNT2 = 0;  /* reset */

			data_output_byte <<= 2;

			if(counter < 80) {
				data_output_byte |= 1;
				total_bits += 2;
			} else if(counter > 111) {
				/* this accounts for just a '1' or a '01' as two '1' arent allowed in a row */
				data_output_byte |= 3;
				total_bits += 4;
			} else {
				data_output_byte |= 2;
				total_bits += 3;
			}

			/* Wait until pin is high again */
			while(!(RDATA_PORT & RDATA_BIT));
		}
		UDR0 = data_output_byte;
	}
	/* Because of the above rules the actual valid two-bit sequences output
	 * are 01, 10 and 11, so we use 00 to say "END OF DATA"
	 */
	write_byte_to_uart(0);

	/* turn off the status LED */
	LED_PORT &= ~LED_BIT;

	/* Disable the counter */
	TCCR2B = 0;	  /* No Clock (turn off) */
}
