#ifndef DEV_H_
#define DEV_H_

int init_device(const char *devname);
void shutdown_device(void);

/* returns non-zero for success, zero for failure, and -1 on comm. error */
int wait_response(void);

int get_fw_version(int *major, int *minor);

#endif	/* DEV_H_ */
