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
#ifndef DEV_H_
#define DEV_H_

int init_device(const char *devname);
void shutdown_device(void);

/* returns non-zero for success, zero for failure, and -1 on comm. error */
int wait_response(void);

int get_fw_version(int *major, int *minor);

int begin_read(void);
int begin_write(void);
int end_access(void);

int select_head(int s);
int move_head(int track);

int read_track(unsigned char *buf);

#endif	/* DEV_H_ */
