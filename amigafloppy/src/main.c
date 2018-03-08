#include <stdio.h>
#include <stdlib.h>
#include "dev.h"
#include "opt.h"

#define TRACK_SIZE		(0x1900 * 2 + 0x440)

int main(int argc, char **argv)
{
	unsigned char buf[TRACK_SIZE];

	if(init_options(argc, argv) == -1) {
		return 1;
	}

	if(init_device(opt.devfile) == -1) {
		return 1;
	}

	begin_read();
	move_head(0);
	select_head(0);
	read_track(buf);
	end_access();

	shutdown_device();
	return 0;
}
