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
#ifndef OPT_H_
#define OPT_H_

struct options {
	char *fname;
	char *devfile;
	int verify;
	int write_disk;
	int verbose;
	int retries;
} opt;

int init_options(int argc, char **argv);

#endif	/* OPT_H_ */
