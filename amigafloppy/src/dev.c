#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "dev.h"
#include "serial.h"
#include "opt.h"

#define TIMEOUT_MSEC	2000
#define TRACK_SIZE		(0x1900 * 2 + 0x440)

static int uncompress(unsigned char *dest, unsigned char *src, int size);
static int align_track(unsigned char *buf, int size);
static void decode_mfm(unsigned char *buf, int size);

static int dev_fd = -1;

int init_device(const char *devname)
{
	int major, minor;

	if((dev_fd = ser_open(devname, 2000000, SER_HWFLOW)) == -1) {
		return -1;
	}
	ser_nonblock(dev_fd);

	if(get_fw_version(&major, &minor) == -1) {
		ser_close(dev_fd);
		dev_fd = -1;
		return -1;
	}
	if(opt.verbose) {
		printf("Firmware version: %d.%d\n", major, minor);
	}

	return dev_fd;
}

void shutdown_device(void)
{
	ser_close(dev_fd);
}

int wait_response(void)
{
	char res;

	if(dev_fd < 0) return -1;

	if(!ser_wait(dev_fd, TIMEOUT_MSEC)) {
		fprintf(stderr, "timeout while waiting for response from device\n");
		return -1;
	}
	if(ser_read(dev_fd, &res, 1) != 1) {
		fprintf(stderr, "failed to read response from device\n");
		return -1;
	}
	return res == '1' ? 1 : 0;
}

static int command(char c)
{
	if(dev_fd < 0) return -1;

	if(ser_write(dev_fd, &c, 1) != 1) {
		fprintf(stderr, "failed to send command to the device\n");
		return -1;
	}
	return wait_response();
}

int get_fw_version(int *major, int *minor)
{
	char buf[5] = {0};

	if(command('?') <= 0) {
		return -1;
	}

	if(ser_read(dev_fd, buf, 4) != 4) {
		fprintf(stderr, "failed to read firmware version\n");
		return -1;
	}

	if(sscanf(buf, "V%d.%d", major, minor) != 2) {
		fprintf(stderr, "got malformed response to version command\n");
		return -1;
	}
	return 0;
}

int begin_read(void)
{
	if(command('+') <= 0) {
		fprintf(stderr, "begin_read failed\n");
		return -1;
	}
	return 0;
}

int begin_write(void)
{
	if(command('~') <= 0) {
		fprintf(stderr, "begin_write failed\n");
		return -1;
	}
	return 0;
}

int end_access(void)
{
	if(command('-') <= 0) {
		fprintf(stderr, "end_access failed\n");
		return -1;
	}
	return 0;
}

int select_head(int s)
{
	if(command(s ? ']' : '[') <= 0) {
		fprintf(stderr, "select_head(%d) failed\n", s);
		return -1;
	}
	return 0;
}

int move_head(int track)
{
	char buf[4];

	if(track > 99) {
		fprintf(stderr, "move_head(%d): invalid track number\n", track);
		return -1;
	}

	if(track <= 0) {
		return command('.');
	}
	sprintf(buf, "#%02d", track);

	ser_write(dev_fd, buf, 3);
	return wait_response();
}

int read_track(unsigned char *resbuf)
{
	unsigned char *ptr, buf[TRACK_SIZE];
	char waitidx = 0;
	int sz, rdbytes, total_read = 0;

	if(command('<') <= 0) {
		return -1;
	}
	ser_write(dev_fd, &waitidx, 1);

	ptr = buf;

	while(total_read < TRACK_SIZE) {
		if(!ser_wait(dev_fd, TIMEOUT_MSEC)) {
			fprintf(stderr, "timeout while reading track\n");
			return -1;
		}
		sz = TRACK_SIZE - total_read;
		if((rdbytes = ser_read(dev_fd, ptr, sz)) <= 0) {
			fprintf(stderr, "failed to read track\n");
			return -1;
		}

		ptr += rdbytes;
		total_read += rdbytes;

		if(!buf[total_read - 1]) {
			printf("total read: %d\n", total_read);
			break;	/* end of data */
		}
	}

	total_read = uncompress(resbuf, buf, total_read);
	printf("total read after decompression: %d\n", total_read);

	if(align_track(resbuf, total_read) == -1) {
	//	return -1;
	}

	decode_mfm(resbuf, total_read);
	return 0;
}

static int uncompress(unsigned char *dest, unsigned char *src, int size)
{
	int i, j;
	int outbits = 0;
	unsigned int val = 0;
	unsigned char *dptr = dest;

	for(i=0; i<size; i++) {
		for(j=0; j<4; j++) {
			int shift = (~j & 3) * 2;
			switch((*src >> shift) & 3) {
			case 1:
				val = (val << 2) | 1;
				outbits += 2;
				break;

			case 2:
				val = (val << 3) | 1;
				outbits += 3;
				break;

			case 3:
				val = (val << 4) | 1;
				outbits += 4;
				break;

			default:
				goto done;
			}

			if(outbits >= 8) {
				*dptr++ = (val >> (outbits - 8)) & 0xff;
				outbits -= 8;

				if(dptr - dest >= TRACK_SIZE) {
					goto done;
				}
			}
		}
		++src;
	}

done:
	return dptr - dest;
}

/* reads at most size + 1 bytes from src and writes size bytes to dest, left-shifted accordingly */
static void copy_bits(unsigned char *dest, unsigned char *src, int size, int shift)
{
	int i;
	if(!shift) {
		memcpy(dest, src, size);
	} else {
		for(i=0; i<size; i++) {
			*dest++ = (src[0] << shift) | (src[1] >> (8 - shift));
			++src;
		}
	}
}

static int align_track(unsigned char *buf, int size)
{
	int i, j, offset, shift = -1;
	unsigned char *ptr = buf;
	unsigned char magic[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x44, 0x89, 0x44, 0x89 };
	unsigned char tmp[sizeof magic];

	for(i=0; i<size - sizeof magic - 1; i++) {	/* -1 because copy_bits reads +1 byte for non-zero shifts */
		for(j=0; j<8; j++) {
			copy_bits(tmp, ptr, sizeof magic, j);
			if(memcmp(tmp + 1, magic + 1, sizeof magic - 1) == 0 &&
					(tmp[0] & 0x7f) == (magic[0] & 0x7f)) {
				shift = j;
				goto end_search;
			}
		}
		++ptr;
	}
end_search:

	if(shift == -1) {
		fprintf(stderr, "failed to locate sector start marker\n");
		return -1;
	}

	offset = ptr - buf;
	printf("align_track: offset %d bytes and %d bits\n", offset, shift);
	copy_bits(buf, ptr, size - offset - (shift ? 1 : 0), shift);
	return 0;
}

static void print_byte(unsigned char val)
{
	printf("%02x ", (unsigned int)val);
}

static void decode_mfm(unsigned char *buf, int size)
{
	int i = 0;
	/* TODO */

	while(size > 0) {
		print_byte(*buf++);
		if(++i == 16) {
			putchar('\n');
			i = 0;
		}
		--size;
	}
	if(i) putchar('\n');
}
