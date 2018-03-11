#ifndef ADF_H_
#define ADF_H_

int adf_open(const char *fname);
void adf_close(void);

int adf_write_track(void *trackbuf);

#endif	/* ADF_H_ */
