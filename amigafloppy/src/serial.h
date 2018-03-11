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
#ifndef SERIAL_H_
#define SERIAL_H_

#define SER_8N1		0
#define SER_8N2		1
#define SER_HWFLOW	2

int ser_open(const char *port, int baud, unsigned int mode);
void ser_close(int fd);

int ser_block(int fd);
int ser_nonblock(int fd);

int ser_pending(int fd);
/* if msec < 0: wait for ever */
int ser_wait(int fd, long msec);

int ser_write(int fd, const void *buf, int count);
int ser_read(int fd, void *buf, int count);

void ser_printf(int fd, const char *fmt, ...);
char *ser_getline(int fd, char *buf, int bsz);

#endif	/* SERIAL_H_ */
