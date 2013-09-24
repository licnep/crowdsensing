/* 
 * File:   SensoreUSB.h
 * Author: alox
 *
 * Created on July 17, 2013, 12:45 AM
 */

#ifndef SENSOREUSB_H
#define	SENSOREUSB_H

#include <libusb-1.0/libusb.h>

class SensoreUSB {
public:
	SensoreUSB(unsigned int vid, unsigned int pid, int ifc, unsigned char ep);
    int init();
    int interruptTransfer();
    double getDust();
    double getTemp();
    double getHum();
    int close();
    SensoreUSB(const SensoreUSB& orig);
    ~SensoreUSB();
private:
    unsigned char data[6];
    double humDouble, tempDouble, dustDouble;
    
    libusb_context *ctx = NULL;
    libusb_device_handle *dev_handle;
	uint16_t vendor_id,product_id;
	unsigned char endpoint;
	int interface_id;
};

#endif	/* SENSOREUSB_H */

