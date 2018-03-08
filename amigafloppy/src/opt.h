#ifndef OPT_H_
#define OPT_H_

struct options {
	char *fname;
	char *devfile;
	int verify;
	int write_disk;
	int verbose;
} opt;

int init_options(int argc, char **argv);

#endif	/* OPT_H_ */
