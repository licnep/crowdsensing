#pragma once

#include <stdio.h>
#include <linux/i2c-dev.h> //userspace i2c communication lib. For usage see: https://www.kernel.org/doc/Documentation/i2c/dev-interface
#include <fcntl.h> //for linux file "open"
#include <unistd.h> //for sleep()
#include <sys/ioctl.h>


class SensoreI2C
{
	public:
	SensoreI2C();
	~SensoreI2C();
	int init();
	int send_measurement_request();
	int humidity_and_temperature_data_fetch(unsigned int *humidity, unsigned int *temperature);

	private:
	int file;
};
