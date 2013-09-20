/* 
 * File:   SensoreUSB.cpp
 * Author: alox
 * 
 * Created on July 17, 2013, 12:45 AM
 */

#include "SensoreUSB.h"
#include <iostream>

#define my_vid 0x04d8
#define my_pid 0x0023
#define ep 0x81

SensoreUSB::SensoreUSB() {
}

int SensoreUSB::init() 
{
    int r;
    r=libusb_init(&ctx);
    if(r!=0){
            printf("Init error\n");
            return 1;
    }
    libusb_set_debug(ctx, 3);
    dev_handle = libusb_open_device_with_vid_pid(ctx, my_vid, my_pid);
    if(dev_handle == NULL)
            printf("Cannot open device\n");
    else
            printf("Device opened\n");
    if(libusb_kernel_driver_active(dev_handle, 0) == 1) {
            printf("Kernel driver active\n");
            if(libusb_detach_kernel_driver(dev_handle, 0) == 0)
                    printf("Kernel driver detached\n");
    }
    r = libusb_claim_interface(dev_handle, 0);
    if(r < 0) {
            printf("Cannot claim interface\n");
            return 1;
    }
    printf("Claimed interface\n\n");
}

/**
 * Funzione bloccante per circa 8ms
 */
int SensoreUSB::interruptTransfer()
{
    int r;
    r = libusb_interrupt_transfer(dev_handle,ep , data, 6, &actual, 0);
    
    if(r == 0 && actual == 6){
            dust=(data[0]<<8)|data[1];
            vdust=dust*4/1024;
            
            dustDouble=(0.172*vdust);//mg/m^3 //qui manca qualcosa
            if((data[2]>>6)==0){
                    hum = data[2];
                    temp = data[4];
                    humDouble = ( (double) ( hum<<8 | data[3] )/16382.0)*100.0;
                    tempDouble=( (double)( temp<<6 |data[5]>>2 )/16382.0)*165.0-40;
            }
            //printf("MISURE\ndensita' polvere: %f mg/m^3\numidità: %d%\ntemperatura: %d°\n\n",dust,hum,temp);
            //s_polveri.aggiungiMisura(dustDouble);
            //s_temp.aggiungiMisura(tempDouble);
            //s_umidita.aggiungiMisura(humDouble);

    }
    else
            printf("USB Read error\n");
    
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
    r = libusb_release_interface(dev_handle, 0);
    if(r!=0) {
            printf("Cannot release interface\n");
            return 1;
    }
    printf("Released interface\n");
    libusb_close(dev_handle);
    printf("Device closed\n");
    libusb_exit(ctx);
    //FINE CHIUSURA USB
}

SensoreUSB::SensoreUSB(const SensoreUSB& orig) {
}

SensoreUSB::~SensoreUSB() {
}

