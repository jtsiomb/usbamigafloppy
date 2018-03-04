#include <stdio.h>
#include <stdlib.h>
#include "dev.h"
#include "opt.h"


int main(int argc, char **argv)
{
	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	if(init_device(opt.devfile) == -1) {
		return 1;
	}

	shutdown_device();
	return 0;
}
