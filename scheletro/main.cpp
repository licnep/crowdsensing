#include <iostream>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "Sensor.h"
#include "SensorReading.h"
#include "SensoreI2C.h"
#include "CrowdSensing.h"
#include <stdio.h>
#include <libusb-1.0/libusb.h>

#define my_vid 0x04d8
#define my_pid 0x0023
#define ep 0x81

#define SIMULATION false

void threadComunicazioneServer(); //definito piu' in basso

using namespace std;

//GLOBAL: (la coda dei dati da inviare, condivisa fra i due thread)
list<SensorReading> lettureDaInviare;
std::mutex mutexLettureDaInviare;
std::condition_variable ciSonoLettureDaInviare;

int main(int argc, char* argv[]) {
	Sensor s_polveri(01,"pm"), s_temp(02,"Celsius"), s_umidita(03,"%"), temp_rasp(11,"Celsius"), umidita_rasp(12,"%");
	
	cout << "Crowdsensing v0.0" << endl;

	//TODO: Inizializza USB e I2C
        //INIZIALIZZAZIONE USB
        
        int i;
	int r,actual;
	unsigned char data[6];
	unsigned short hum=0,temp=0;
	float dust=0,vdust;

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
        
        //////////////////// FINE USB
        

        SensoreI2C sensoreInterno;
        unsigned int umiditaInterna, temperaturaInterna, cicliDaUltimaRichiestaTemp = 0;

        if (!SIMULATION)
        {
            //Inizializzazione I2C
            int error = sensoreInterno.init();
            if (error!=0) 
            {	
                    cout << "Errore inizializzazione i2c. Uscita forzata." << endl;
                    exit(1);
            }
        }
            
	//avvio il thread di comunicazione col server
	std::thread thread2(threadComunicazioneServer);

	auto ultimoSalvataggio = std::chrono::system_clock::now();
	while(1) //while dell'USB (ogni 8ms)
	{
		//simuliamo l'attesa di 8ms, in produzione e' il codice bloccante dell'usb che ci fa aspettare
		if(SIMULATION) std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
                else r = libusb_interrupt_transfer(dev_handle,ep , data, 6, &actual, 0);
		
		//TODO: leggi tutti i sensori
                
                //LETTURA USB
                
                if(r == 0 && actual == 6){
                        double humDouble, tempDouble, dustDouble;
			dust=(data[0]<<8)|data[1];
			vdust=dust*4/1024;
			dustDouble=(0.172*vdust);//qui manca qualcosa
			if((data[2]>>6)==0){
				hum = data[2];
                                temp = data[4];
                                humDouble = ( (double) ( hum<<8 | data[3] )/16382.0)*100.0;
                                tempDouble=( (double)( temp<<6 |data[5]>>2 )/16382.0)*165.0-40;
			}
			//printf("MISURE\ndensita' polvere: %f mg/m^3\numidità: %d%\ntemperatura: %d°\n\n",dust,hum,temp);
                        s_polveri.aggiungiMisura(dustDouble);
                        s_temp.aggiungiMisura(tempDouble);
                        s_umidita.aggiungiMisura(humDouble);
                        
		}
		else
			printf("Read error\n");
                
                //FINE LETTURA USB

		cicliDaUltimaRichiestaTemp++;
		if (cicliDaUltimaRichiestaTemp >=4)
		{
                        int result = sensoreInterno.humidity_and_temperature_data_fetch(&umiditaInterna,&temperaturaInterna);
                        if (SIMULATION) {
                            umiditaInterna = 50;temperaturaInterna = 100;
                        }
			if (result==0||SIMULATION) 	//misura andata a buon fine, no stale data o altro
			{
				temp_rasp.aggiungiMisura(temperaturaInterna*165.0/16382 - 40);
				umidita_rasp.aggiungiMisura(umiditaInterna*100.0/16382);
				//TODO: sta roba andrebbe fatta nella classe sensoreI2C
				/*cout << "Temperatura media: " << temp_rasp.media() << " C" << endl;
				cout << "Varianza: " << temp_rasp.varianza() << " C^2" << endl;
				cout << "Umidita media: " << umidita_rasp.media() << " %" << endl;
				cout << "Varianza: " << umidita_rasp.varianza() << endl;*/
			}
			sensoreInterno.send_measurement_request();
			cicliDaUltimaRichiestaTemp = 0;
		}

		auto millisecondiDaUltimoSalvataggio = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - ultimoSalvataggio).count();
		if (millisecondiDaUltimoSalvataggio > 5000) //5*60*1000=5 minuti
		{
			ultimoSalvataggio = std::chrono::system_clock::now();
			{
				//ottengo il mutex per scrivere nella coda
				std::lock_guard<std::mutex> l(mutexLettureDaInviare);
				//TODO: forse si potrebbe usare try_lock_for() per evitare di perdere la sincronizzazione usb
				//aggiunge tutti i valori alla coda
				lettureDaInviare.push_front(SensorReading(s_polveri));
				lettureDaInviare.push_front(SensorReading(s_temp));
				lettureDaInviare.push_front(SensorReading(s_umidita));
				lettureDaInviare.push_front(SensorReading(temp_rasp));
				lettureDaInviare.push_front(SensorReading(umidita_rasp));
			}
			
			//notifico l'altro thread che c'e' roba da inviare in coda:
			ciSonoLettureDaInviare.notify_one();

			//resetto i conteggi per media e varianza
			polveri.reset();
			temp.reset();
			umidita.reset();
			temp_rasp.reset();
			umidita_rasp.reset();
		}
	}

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
        
	thread2.join();
	return 0;
}


void threadComunicazioneServer()
{
	list<SensorReading> listaLocale;
        
        //inizializzazione CrowdSensing
        CrowdSensing cs("b8:27:eb:69:a4:20","gruppo35","password");
        cs.addFeed(11,"raspberry, internal, temperature");
        cs.addFeed(12,"raspberry, internal, humidity");
        
	while(1)
	{
		//acquisisco il mutex
		std::unique_lock<std::mutex> ul(mutexLettureDaInviare);
		while (lettureDaInviare.empty())
		{
			//rilascia il mutex e attende (senza consumo risorse) che ci siano nuovi dati da inviare
			//quando e' svegliato dalla condizione acquisice il mutex automaticamente
			ciSonoLettureDaInviare.wait(ul);
		}
		//copia tutti i dati da inviare nella coda locale, e svuota lettureDaInviare
		listaLocale.splice(listaLocale.begin(),lettureDaInviare);
		//rilascio il mutex
		ul.unlock();

		if (!listaLocale.empty())
		{
                    //TODO: prova a inviare al server tutti i rilevamenti contenuti nella listaLocale
                    cs.inviaRilevazioni(listaLocale);
		}
	}
}
