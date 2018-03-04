#include <stdio.h>
#include "dev.h"
#include "serial.h"
#include "opt.h"

#define TIMEOUT_MSEC	2000

static int dev_fd = -1;

int init_device(const char *devname)
{
	int major, minor;

	if((dev_fd = ser_open(devname, 2000000, SER_HWFLOW)) == -1) {
		return -1;
	}

	if(get_fw_version(&major, &minor) == -1) {
		ser_close(dev_fd);
		dev_fd = -1;
		return -1;
	}
	if(opt.verbose) {
		printf("Firmware version: %d.%d\n", major, minor);
	}

	return dev_fd;
}

void shutdown_device(void)
{
	ser_close(dev_fd);
}

int wait_response(void)
{
	char res;

	if(dev_fd < 0) return -1;

	if(!ser_wait(dev_fd, TIMEOUT_MSEC)) {
		fprintf(stderr, "timeout while waiting for response from device\n");
		return -1;
	}
	if(ser_read(dev_fd, &res, 1) != 1) {
		fprintf(stderr, "failed to read response from device\n");
		return -1;
	}
	return res == '1' ? 1 : 0;
}

static int command(char c)
{
	if(dev_fd < 0) return -1;

	if(ser_write(dev_fd, &c, 1) != 1) {
		fprintf(stderr, "failed to send command to the device\n");
		return -1;
	}
	return wait_response();
}

int get_fw_version(int *major, int *minor)
{
	char buf[5] = {0};

	if(command('?') <= 0) {
		return -1;
	}

	if(ser_read(dev_fd, buf, 4) != 4) {
		fprintf(stderr, "failed to read firmware version\n");
		return -1;
	}

	if(sscanf(buf, "V%d.%d", major, minor) != 2) {
		fprintf(stderr, "got malformed response to version command\n");
		return -1;
	}
	return 0;
}
