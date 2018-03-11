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
