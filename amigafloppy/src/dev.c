/*
amigafloppy - driver for the USB floppy controller for amiga disks
Copyright (C) 2018  John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "dev.h"
#include "serial.h"
#include "opt.h"

#ifdef __GNUC__
#define PACKED	__attribute__ ((packed))
#else
#define PACKED
#endif

#define MFM_HDR_FMT_OFFSET	(offsetof(struct sector_header, fmt) * 2)
#define MFM_HDR_HSUM_OFFSET	(offsetof(struct sector_header, hdr_sum) * 2)
#define MFM_HDR_DSUM_OFFSET	(offsetof(struct sector_header, data_sum) * 2)
#define MFM_DATA_OFFSET		(sizeof(struct sector_header) * 2)

struct sector_header {
	unsigned char magic[4];
	unsigned char fmt;
	unsigned char track;
	unsigned char sector;
	unsigned char sec_to_gap;
	unsigned char osinfo[16];
	uint32_t hdr_sum, data_sum;
} PACKED;

struct sector_node {
	struct sector_header hdr;
	unsigned char *rawptr;
	struct sector_node *next;
};

#define TIMEOUT_MSEC	2000
#define TRACK_SIZE		(0x1900 * 2 + 0x440)
#define SECTORS_PER_TRACK	11

static int uncompress(unsigned char *dest, unsigned char *src, int size);
static int align_track(unsigned char *buf, int size);
static struct sector_node *find_sectors(unsigned char *buf, int size);
static void debug_print(unsigned char *dest, int size);
static void dbg_print_header(struct sector_header *hdr);
static void decode_mfm(unsigned char *dest, unsigned char *src, int blksz);

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
	if(command(s ? '[' : ']') <= 0) {
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
	int i, sz, rdbytes, total_read = 0;
	struct sector_node *slist;

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
			break;	/* end of data */
		}
	}

	total_read = uncompress(resbuf, buf, total_read);

	/* move uncompressed data back to the temporary buffer */
	memcpy(buf, resbuf, total_read);

	if(align_track(buf, total_read) == -1) {
		return -1;
	}

	if(!(slist = find_sectors(buf, total_read))) {
		fprintf(stderr, "failed to construct sector list\n");
		return -1;
	}

	/* TODO: checksums */

	ptr = resbuf;
	for(i=0; i<SECTORS_PER_TRACK; i++) {
		struct sector_node *sec = slist;
		while(sec) {
			if(sec->hdr.sector == i) {
				decode_mfm(ptr, sec->rawptr + MFM_DATA_OFFSET, 512);
				ptr += 512;
				break;
			}
			sec = sec->next;
		}
	}

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

static const unsigned char magic[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x44, 0x89, 0x44, 0x89 };

static int check_magic(unsigned char *buf)
{
	return memcmp(buf + 1, magic + 1, sizeof magic - 1) == 0 &&
		(buf[0] & 0x7f) == (magic[0] & 0x7f);
}

static int align_track(unsigned char *buf, int size)
{
	int i, j, offset, shift = -1;
	unsigned char *ptr = buf;
	unsigned char tmp[sizeof magic];

	for(i=0; i<size - sizeof magic - 1; i++) {	/* -1 because copy_bits reads +1 byte for non-zero shifts */
		for(j=0; j<8; j++) {
			copy_bits(tmp, ptr, sizeof magic, j);
			if(check_magic(tmp)) {
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
	/*printf("align_track: offset %d bytes and %d bits\n", offset, shift);*/
	copy_bits(buf, ptr, size - offset - (shift ? 1 : 0), shift);

	return 0;
}

struct sector_node *find_sectors(unsigned char *buf, int size)
{
	unsigned char *ptr = buf;
	struct sector_node *node, *head = 0, *tail = 0;
	int nfound = 0;

	while(ptr - buf < size && nfound < SECTORS_PER_TRACK) {
		if(check_magic(ptr)) {
			if(!(node = malloc(sizeof *node))) {
				fprintf(stderr, "failed to allocate memory for sector list\n");
				goto err;
			}
			decode_mfm((unsigned char*)&node->hdr.fmt, ptr + MFM_HDR_FMT_OFFSET, 4);
			decode_mfm((unsigned char*)&node->hdr.hdr_sum, ptr + MFM_HDR_HSUM_OFFSET, 4);
			decode_mfm((unsigned char*)&node->hdr.data_sum, ptr + MFM_HDR_DSUM_OFFSET, 4);
			node->rawptr = ptr;
			node->next = 0;

			if(head) {
				tail->next = node;
				tail = node;
			} else {
				head = tail = node;
			}
			ptr += (512 + sizeof node->hdr) * 2;
			++nfound;
		} else {
			++ptr;
		}
	}

	return head;
err:
	while(head) {
		void *tmp = head;
		head = head->next;
		free(tmp);
	}
	return 0;
}

static void print_byte(unsigned char val)
{
	printf("%02x ", (unsigned int)val);
}

static void debug_print(unsigned char *buf, int size)
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

static void dbg_print_header(struct sector_header *hdr)
{
	printf("Sector header\n");
	printf("  format: %x\n", (unsigned int)hdr->fmt);
	printf("  track: %u\n", (unsigned int)hdr->track);
	printf("  sector: %u\n", (unsigned int)hdr->sector);
	printf("  sectors before gap: %u\n", (unsigned int)hdr->sec_to_gap);
	printf("  header checksum: %lu\n", (unsigned long)hdr->hdr_sum);
	printf("  data checksum: %lu\n", (unsigned long)hdr->data_sum);
}

/* this can work in-place (dest == src) */
static void decode_mfm(unsigned char *dest, unsigned char *src, int blksz)
{
	int i, j;

	for(i=0; i<blksz; i++) {
		unsigned char even = src[blksz];
		unsigned char odd = *src++;

		for(j=0; j<4; j++) {
			*dest <<= 2;
			if(even & 0x40) {
				*dest |= 1;
			}
			if(odd & 0x40) {
				*dest |= 2;
			}
			even <<= 2;
			odd <<= 2;
		}
		++dest;
	}
}
