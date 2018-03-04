#include <stdio.h>
#include <stdlib.h>
#include "opt.h"

static void print_usage(const char *argv0);

#ifndef WIN32
#define DEVFILE_FMT	"/dev/ttyUSB%d"
#define DEV_DEFAULT "/dev/ttyUSB0"
#else
#define DEVFILE_FMT	"COM%d"
#define DEV_DEFAULT "COM3"
#endif


int parse_args(int argc, char **argv)
{
	int i, num;
	char *endp;
	static char devbuf[16];

	opt.devfile = DEV_DEFAULT;
	opt.verbose = 1;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(argv[i][2] == 0) {
				switch(argv[i][1]) {
				case 'w':
					opt.write_disk = 1;
					break;

				case 'v':
					opt.verify = 1;
					break;

				case 'd':
					num = strtol(argv[++i], &endp, 10);
					if(endp != argv[i]) {
						opt.devfile = argv[i];
					} else {
						sprintf(devbuf, DEVFILE_FMT, num);
						opt.devfile = devbuf;
					}
					break;

				case 's':
					opt.verbose = 0;
					break;

				case 'h':
					print_usage(argv[0]);
					exit(0);

				default:
					fprintf(stderr, "invalid option: %s\n\n", argv[i]);
					print_usage(argv[0]);
					return -1;
				}
			} else {
				fprintf(stderr, "invalid option: %s\n\n", argv[i]);
				print_usage(argv[0]);
				return -1;
			}

		} else {
			if(opt.fname) {
				fprintf(stderr, "unexpected argument: %s\n\n", argv[i]);
				print_usage(argv[0]);
				return -1;
			}
			opt.fname = argv[i];
		}
	}

	if(!opt.fname) {
		fprintf(stderr, "you need to specify the ADF image filename\n");
		return -1;
	}
	return 0;
}


static void print_usage(const char *argv0)
{
	printf("Usage: %s [options] <amiga disk image>\n", argv0);
	printf("Options:\n");
	printf(" -w           write ADF image to disk (default: read from disk)\n");
	printf(" -v           verify after writing (default: no verification)\n");
	printf(" -d <device>  specify which device to use (default: " DEV_DEFAULT ")\n");
	printf(" -s           run silent, print only errors\n");
	printf(" -h           print help and exit\n");
}
