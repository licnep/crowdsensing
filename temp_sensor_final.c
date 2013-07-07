#include <stdio.h>
#include <linux/i2c-dev.h> //userspace i2c communication lib. For usage see: https://www.kernel.org/doc/Documentation/i2c/dev-interface
#include <fcntl.h> //for linux file "open"
#include <unistd.h> //for sleep()

int main() 
{
	int adapter_nr = 1;
	char filename[20];
	snprintf(filename, 19, "/dev/i2c-%d",adapter_nr);
	int result = -1;
	
	/*
	 *  OPEN THE I2C INTERFACE
	 */
	int file = open(filename, O_RDWR);
	if (file < 0) 
	{
		fprintf(stderr, "ERROR: Can't open %s (are you root? did you run 'modprobe i2c-dev'?)\n",filename);
		return(1);
	}


	/*
	 *  SELECT SLAVE 0x27 (the humidity sensor)
	 */
	int addr = 0x27; //humidity sensor address
	if (ioctl(file,I2C_SLAVE, addr) < 0) //change the slave address
	{
		fprintf(stderr, "ERROR: cannot select slave n.%d\n",addr);
		return(1);
	}


	/*
	 * SEND MEASUREMENT REQUEST
	 */
	int i=0;
	for (i=0;i<20;i++)
	{
		result = send_measurement_request(file);
		if (result < 0) return -1;
	
		sleep(1); //need about 36ms to perform a measurement
	
		unsigned int humidity, temperature;
		result = humidity_and_temperature_data_fetch(file,&humidity,&temperature);
		if (result < 0) return -1;
	
		printf("UMIDITA': %d\%\n",humidity*100/16382);
		printf("TEMP    : %d C\n",temperature*165/16382 - 40);
	}

	close(file);
	return 0;
}


/**
	@returns -1 on error, 0 on success
*/
int send_measurement_request(int file)
{
	//the measurement request is just an empty write message
	char buffer[1] = {0};
	ssize_t written = write(file,buffer,0); //write 0 bytes
        if (written < 0)
        {
                fprintf(stderr, "ERROR: failed sending measurement request (empty write message)\n");
                return(-1);
	}
	return 0;
}


/**
	humidity must be divided by 2^14-2 to get the percentage.
	temperature must be divided by 2^14-2 and normalized between -40C and 125C
	@returns -1 on error, 0 on success, 1 on stale data. Writes the readings in *humidity and *temperature
*/
int humidity_and_temperature_data_fetch(int file, unsigned int *humidity, unsigned int *temperature)
{
	*humidity = *temperature = 0xFFFF; //set humidity and temp to invalid value in case the reading fails
	
	unsigned char buffer[4] = {255,255,255,255};

        ssize_t readLength = read(file, buffer,sizeof(unsigned char)*4);
        if (readLength < 0)
        {
                fprintf(stderr, "ERROR: reading failed.\n");
                return(-1);
        }

	/*
	 * read the status [00]=valid data  [01]=stale data
	 */
	unsigned char status = buffer[0] >> 6; //right shift 6 times so status contains only the first 2 bits
	if (status==0b0000001)
	{
		fprintf(stderr, "WARNING: stale data\n"); //you should read later or send a measurement request if you haven't
		return 1;
	}
	else if (status==0b00000010)
	{
		fprintf(stderr, "ERROR: device in Command Mode\n");
		return -1;
	}
	else if (status!=0)
	{
		fprintf(stderr, "ERROR: unknown status %d!\n",status);
		return -1;
	}

	//if we are here status=0. Data is valid and can be parsed.

	*humidity = ((buffer[0] & 0b00111111) << 8) | buffer[1];
	*temperature = (buffer[2] << 6) | (buffer[3] >> 2);


	return 0;
}
