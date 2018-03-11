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
#include <string.h>
#include <errno.h>
#include "adf.h"

static FILE *fp;

int adf_open(const char *fname)
{
	if(fp) return -1;

	if(!(fp = fopen(fname, "wb"))) {
		fprintf(stderr, "failed to open %s for writing: %s\n", fname, strerror(errno));
		return -1;
	}

	return 0;
}

void adf_close(void)
{
	if(fp) {
		fclose(fp);
		fp = 0;
	}
}

int adf_write_track(void *trackbuf)
{
	if(!fp) return -1;
	return fwrite(trackbuf, 512, 11, fp) == 11 ? 0 : -1;
}
