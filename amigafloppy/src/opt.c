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
#include <string.h>
#include <ctype.h>
#include <alloca.h>
#include <unistd.h>
#include <pwd.h>
#include "opt.h"

static void print_usage(const char *argv0);
static int load_config(void);
static char *skip_wspace(char *s);
static char *cleanstr(char *s);
static int strbool(char *s);

#ifndef WIN32
#define DEVFILE_FMT	"/dev/ttyUSB%d"
#define DEV_DEFAULT "/dev/ttyUSB0"
#else
#define DEVFILE_FMT	"COM%d"
#define DEV_DEFAULT "COM3"
#endif


int init_options(int argc, char **argv)
{
	int i, num;
	char *endp;
	static char devbuf[16];

	opt.devfile = DEV_DEFAULT;
	opt.verbose = 1;

	load_config();

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
					if(endp == argv[i]) {
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


static int load_config(void)
{
	FILE *fp;
	int val;
	char buf[512], *line, *endp, *valstr;
	char *fname;

	if((fp = fopen("amigafloppy.conf", "r"))) {
		fname = alloca(20);
		strcpy(fname, "amigafloppy.conf");
	} else {
		char *env;
		struct passwd *pw;

		if((pw = getpwuid(getuid()))) {
			sprintf(buf, "%s/.amigafloppy.conf", pw->pw_dir);
		} else if((env = getenv("HOME"))) {
			sprintf(buf, "%s/.amigafloppy.conf", env);
		} else {
			return -1;
		}
		if(!(fp = fopen(buf, "r"))) {
			return -1;
		}
		fname = alloca(strlen(buf) + 1);
		strcpy(fname, buf);
	}

	while(fgets(buf, sizeof buf, fp)) {
		line = skip_wspace(buf);
		if((endp = strchr(line, '#'))) {
			*endp = 0;
		}
		if(!*line) continue;
		if(!(endp = strchr(line, '=')) || !*(valstr = cleanstr(endp + 1))) {
			fprintf(stderr, "config file: %s: invalid line: %s\n", fname, line);
			continue;
		}
		*endp = 0;
		line = cleanstr(line);

		if(strcasecmp(line, "verify") == 0) {
			if((val = strbool(valstr)) == -1) {
				fprintf(stderr, "config file: %s: verify must be followed by a boolean value (found: %s)\n", fname, valstr);
				continue;
			}
			opt.verify = val;

		} else if(strcasecmp(line, "device") == 0) {
			if(!(opt.devfile = malloc(strlen(valstr) + 1))) {
				fprintf(stderr, "failed to allocate device filename buffer (%s)\n", valstr);
				abort();
			}
			strcpy(opt.devfile, valstr);

		} else {
			fprintf(stderr, "config file: %s: invalid option: %s\n", fname, line);
		}
	}

	fclose(fp);
	return 0;
}

static char *skip_wspace(char *s)
{
	while(*s && isspace(*s)) s++;
	return s;
}

static char *cleanstr(char *s)
{
	char *endp;
	if(!*(s = skip_wspace(s))) return s;
	endp = s + strlen(s) - 1;
	while(endp > s && isspace(*endp)) endp--;
	endp[1] = 0;
	return s;
}

static int strbool(char *s)
{
	if(s[1] == 0) {
		if(*s == '1') return 1;
		if(*s == '0') return 0;
		return -1;
	}
	if(strcasecmp(s, "true") == 0 || strcasecmp(s, "yes") == 0 || strcasecmp(s, "on") == 0) {
		return 1;
	}
	if(strcasecmp(s, "false") == 0 || strcasecmp(s, "no") == 0 || strcasecmp(s, "off") == 0) {
		return 0;
	}
	return -1;
}
