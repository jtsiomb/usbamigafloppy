#ifndef DEV_H_
#define DEV_H_

int init_device(const char *devname);
void shutdown_device(void);

/* returns non-zero for success, zero for failure, and -1 on comm. error */
int wait_response(void);

int get_fw_version(int *major, int *minor);

int begin_read(void);
int begin_write(void);
int end_access(void);

int select_head(int s);
int move_head(int track);

int read_track(unsigned char *buf);

#endif	/* DEV_H_ */
