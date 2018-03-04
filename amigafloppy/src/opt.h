#ifndef OPT_H_
#define OPT_H_

struct options {
	const char *fname;
	const char *devfile;
	int verify;
	int write_disk;
	int verbose;
} opt;

int parse_args(int argc, char **argv);

#endif	/* OPT_H_ */
