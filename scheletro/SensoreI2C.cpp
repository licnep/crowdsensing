#include "SensoreI2C.h"

SensoreI2C::SensoreI2C()
{
	this->file = -1;
}

SensoreI2C::~SensoreI2C()
{
	close(this->file);
}

/**
	Inizializza l'I2C.
	@returns 0 on success, 1 on error (errors are printed on stderr)
*/
int SensoreI2C::init()
{
	int adapter_nr = 1;
	char filename[20];
	snprintf(filename, 19, "/dev/i2c-%d",adapter_nr);
	int result = -1;
	
	/*
	 *  OPEN THE I2C INTERFACE
	 */
	this->file = open(filename, O_RDWR);
	if (file < 0) 
	{
		std::cerr << "ERROR: Can't open " << filename << " (are you root? did you run 'modprobe i2c-dev'?)\n";
		return(1);
	}


	/*
	 *  SELECT SLAVE 0x27 (the humidity sensor)
	 */
	int addr = 0x27; //humidity sensor address
	if (ioctl(file,I2C_SLAVE, addr) < 0) //change the slave address
	{
		std::cerr << "ERROR: cannot select slave n." << addr << std::endl;
		return(1);
	}
	return 0;
}


/**
	@returns -1 on error, 0 on success
*/
int SensoreI2C::send_measurement_request()
{
	if (this->file<1) return -1;

	//the measurement request is just an empty write message
	char buffer[1] = {0};
	ssize_t written = write(file,buffer,0); //write 0 bytes
    if (written < 0)
    {
            std::cerr << "ERROR: failed sending measurement request (empty write message)\n";
            return(-1);
	}
	return 0;
}


/**
	@returns -1 on error, 0 on success, 1 on stale data. Writes the readings in *humidity and *temperature
*/
int SensoreI2C::humidity_and_temperature_data_fetch(double *humidity, double *temperature)
{
	unsigned int _humidity, _temperature;

	_humidity = _temperature = 0xFFFF; //set humidity and temp to invalid value in case the reading fails
	
	unsigned char buffer[4] = {255,255,255,255};

        ssize_t readLength = read(file, buffer,sizeof(unsigned char)*4);
        if (readLength < 0)
        {
                std::cerr << "ERROR: reading failed.\n";
                return(-1);
        }

	/*
	 * read the status [00]=valid data  [01]=stale data
	 */
	unsigned char status = buffer[0] >> 6; //right shift 6 times so status contains only the first 2 bits
	if (status==0b0000001)
	{
		//fprintf(stderr, "WARNING: stale data\n"); //you should read later or send a measurement request if you haven't
		return -1;
	}
	else if (status==0b00000010)
	{
		std::cerr << "ERROR: device in Command Mode\n";
		return -1;
	}
	else if (status!=0)
	{
		std::cerr << "ERROR: unknown status " << status << "!\n";
		return -1;
	}

	//if we are here status=0. Data is valid and can be parsed.

	_humidity = ((buffer[0] & 0b00111111) << 8) | buffer[1];
	_temperature = (buffer[2] << 6) | (buffer[3] >> 2);

	//conversion to double.
	//humidity must be divided by 2^14-2 to get the percentage.
	//temperature must be divided by 2^14-2 and normalized between -40C and 125C

	*temperature = _temperature*165.0/16382 - 40;
	*humidity = _humidity*100.0/16382;

	return 0;
}

