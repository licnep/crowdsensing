/* 
 * File:   SensoreUSB.cpp
 * Author: alox
 * 
 * Created on July 17, 2013, 12:45 AM
 */

#include "SensoreUSB.h"
#include <iostream>

/*#define my_vid 0x04d8
#define my_pid 0x0023
#define ep 0x81*/

SensoreUSB::SensoreUSB(unsigned int vid, unsigned int pid, int ifc, unsigned char ep) {
	vendor_id=vid;
	product_id=pid;
	interface_id=ifc;
	endpoint=ep;

	int r;
	 r=libusb_init(&ctx);
    if(r!=0){
            std::cerr << "Init error\n";
            exit(1);
    }
	if(!init())
		exit(0);

}

int SensoreUSB::init() 
{
    dev_handle = libusb_open_device_with_vid_pid(ctx, vendor_id,product_id);
    if(dev_handle == NULL){
            std::cerr << "Cannot open device\n";
			return 0;
	}
    else
            std::cout << "Device opened\n";

	if(libusb_kernel_driver_active(dev_handle, interface_id) == 1) {
            std::cout << "Kernel driver active\n";
			if(libusb_detach_kernel_driver(dev_handle,interface_id) == 0)
                    std::cout << "Kernel driver detached\n";
			else{ 
					std::cerr << "Cannot detach kernel driver\n";
					return 0;
			}
    }

	int r = libusb_claim_interface(dev_handle, interface_id);
    if(r!=0) {
            std::cerr << "Cannot claim interface\n";
            return 1;
    }
    std::cout << "Claimed interface\n\n";
}

/**
 * Funzione bloccante per circa 8ms
 */
int SensoreUSB::interruptTransfer()
{
    int r,actual;
	float dust;
	unsigned short hum, temp;
	
	r = libusb_interrupt_transfer(dev_handle,endpoint, data, 6, &actual, 0);
    
    if(r == 0 && actual == 6){				
            dust=(data[0]<<8)|data[1];
            dust=dust*4.096/1024;
            dustDouble=(0.172*dust);//mg/m^3
            if((data[2]>>6)==0){
                    hum = data[2];
                    temp = data[4];
                    humDouble = ( (double) ( hum<<8 | data[3] )/16382.0)*100.0;
                    tempDouble=( (double)( temp<<6 |data[5]>>2 )/16382.0)*165.0-40;
            }
	}
	else if(r==LIBUSB_ERROR_NO_DEVICE){
			std::cerr << "Read error. Device deatched\n";
			//se si stacca l'usb restiamo in attesa finche' non viene riattaccata
			while(!init());
	}
	else
            std::cerr << "USB Read error\n";
    
}


double SensoreUSB::getDust() 
{
    return dustDouble;
}
double SensoreUSB::getTemp()
{
    return tempDouble;
}
double SensoreUSB::getHum()
{
    return humDouble;
}

int SensoreUSB::close() 
{
    int r;
    //CHIUSURA USB

	/*int usb_release_interface(usb_dev_handle *dev, int interface)
	rilascia un'interfaccia precedentemente richiesta mediante la funzione usb_claim_interface.
	La funzione ritorna 0 se il rilascio avviene con successo. 
	*/
    r = libusb_release_interface(dev_handle, 0);
    if(r!=0) {
            std::cerr << "Cannot release interface\n";
            return 1;
    }
    std::cout << "Released interface\n";

	/*void libusb_close	(libusb_device_handle * dev_handle)
	chiude l'handle a un dispositivo.
	*/
    libusb_close(dev_handle);
    std::cout << "Device closed\n";

	/*void libusb_exit	(struct libusb_context * 	ctx	)
	termina la sessione libusb.
	*/
    libusb_exit(ctx);
    //FINE CHIUSURA USB
}

SensoreUSB::SensoreUSB(const SensoreUSB& orig) {
}

SensoreUSB::~SensoreUSB() {
}

