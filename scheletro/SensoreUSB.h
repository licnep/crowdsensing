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
    SensoreUSB();
    int init();
    int interruptTransfer();
    double getDust();
    double getTemp();
    double getHum();
    int close();
    SensoreUSB(const SensoreUSB& orig);
    virtual ~SensoreUSB();
private:
    unsigned char data[6];
    unsigned short hum=0,temp=0;
    float dust=0,vdust;
    int actual;
    double humDouble, tempDouble, dustDouble;
    
    libusb_context *ctx = NULL;
    libusb_device_handle *dev_handle;
};

#endif	/* SENSOREUSB_H */

