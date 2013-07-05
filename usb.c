#include <stdio.h>
#include <libusb-1.0/libusb.h>

#define my_vid 0x04d8
#define my_pid 0x0023
#define ep 0x81


int main(){
	int i;
	int r,actual;
	unsigned char data[6];
	unsigned short dust=0,hum=0,temp=0;

	libusb_context *ctx = NULL;
	libusb_device_handle *dev_handle;
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
	while(1){
	//for(i=0;i<1;i++){
		r = libusb_interrupt_transfer(dev_handle,ep , data, 6, &actual, 0);

		if(r == 0 && actual == 6){
			dust=(data[0]<<8)|data[1];
			if((data[2]>>6)==0){
				hum=((data[2]<<8)|data[3])/((2^14)-2);
				temp=(((((data[3]<<8)|data[4])>>2)*165)/((2^14)-2))-40;
			}
			printf("MISURE\ndensita' polvere: %d\numidità: %d%\ntemperatura: %d°\n\n",dust,hum,temp);
		}
		else
			printf("Read error\n");
	}

	r = libusb_release_interface(dev_handle, 0);
	if(r!=0) {
		printf("Cannot release interface\n");
		return 1;
	}
	printf("Released interface\n");
	libusb_close(dev_handle);
	printf("Device closed\n");
	libusb_exit(ctx);
	return 0;
}



