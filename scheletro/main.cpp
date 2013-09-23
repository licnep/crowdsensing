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
#include "SensoreUSB.h"
#include <stdio.h>
#include <libusb-1.0/libusb.h>

void threadComunicazioneServer(); //definito piu' in basso

using namespace std;

//GLOBAL: (la coda dei dati da inviare, condivisa fra i due thread)
list<SensorReading> lettureDaInviare;
std::mutex mutexLettureDaInviare;
std::condition_variable ciSonoLettureDaInviare;

int main(int argc, char* argv[]) {
    
    CrowdSensing cs("80:1f:02:87:82:84","gruppo35","034FpK69l4",true); //true significa che comunichiamo con la versione non test
    cs.getLocation();

    return 0;
    

	Sensor s_polveri(01,"mg/m^3"), s_temp(02,"Celsius"), s_umidita(03,"%"), temp_rasp(11,"Celsius"), umidita_rasp(12,"%");
	
	cout << "Crowdsensing v0.0" << endl;
        
    //Inizializzazione sensore USB
    SensoreUSB sensoreUSB;
    sensoreUSB.init();
    
    //Inizializzazione sensore I2C
    SensoreI2C sensoreInterno;
    //la variabile cicliDaUltimaRichiestaTemp e' usata perche' la temperatura non la richiediamo ad ogni ciclo
    unsigned int cicliDaUltimaRichiestaTemp = 0;

    int error = sensoreInterno.init();
    if (error!=0) 
    {	
            cout << "Errore inizializzazione i2c. Uscita forzata." << endl;
            exit(1);
    }
            
	//avvio il thread di comunicazione col server
	std::thread thread2(threadComunicazioneServer);

	//variabile in cui memorizzo il timestamp dell'ultimo invio, quando sono passati 5 minuti dall'ultimo invio faccio un nuovo invio
	auto ultimoSalvataggio = std::chrono::system_clock::now();

	while(1) //while dell'USB (ogni 8ms)
	{
        //funzione bloccante (attesa di 8ms)
        sensoreUSB.interruptTransfer();
                
        s_polveri.aggiungiMisura(sensoreUSB.getDust());
        s_temp.aggiungiMisura(sensoreUSB.getTemp());
        s_umidita.aggiungiMisura(sensoreUSB.getHum());
        
        //FINE LETTURA USB

        //Lettura sensore interno I2C
		cicliDaUltimaRichiestaTemp++;
		if (cicliDaUltimaRichiestaTemp > 5)
		{
			double umiditaInterna, temperaturaInterna;
            int result = sensoreInterno.humidity_and_temperature_data_fetch(&umiditaInterna,&temperaturaInterna);
			if (result==0) 	//misura andata a buon fine, no stale data o altro
			{
				temp_rasp.aggiungiMisura(temperaturaInterna);
				umidita_rasp.aggiungiMisura(umiditaInterna);
			}
			sensoreInterno.send_measurement_request();
			cicliDaUltimaRichiestaTemp = 0;
		}

		auto millisecondiDaUltimoSalvataggio = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - ultimoSalvataggio).count();
		if (millisecondiDaUltimoSalvataggio > 5*60*1000) //5*60*1000=5 minuti
		{
			ultimoSalvataggio = std::chrono::system_clock::now();
			{
				//ottengo il mutex per scrivere nella coda
				std::lock_guard<std::mutex> l(mutexLettureDaInviare);
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
			s_polveri.reset();
			s_temp.reset();
			s_umidita.reset();
			temp_rasp.reset();
			umidita_rasp.reset();
		}
	}

    sensoreUSB.close();
        
	thread2.join();
	return 0;
}


void threadComunicazioneServer()
{
	//codaLocale e' una lista di liste, ogni sottolista contiene un gruppo di rilevazioni fatte in uno stesso momento dai vari sensori
    //la superlista e' invece usata come coda per tenere i dati in memoria in caso di disconnessioni o invii falliti, e riprovare + tardi
	list<list<SensorReading>> codaLocale;
        
    //inizializzazione CrowdSensing
    CrowdSensing cs("80:1f:02:87:82:84","gruppo35","034FpK69l4",true); //true significa che comunichiamo con la versione non test

    cs.checkAPIVersion();
    cs.getLocation();

    //try to register this device if it isn't registered already
    int device_id = cs.getDeviceIDFromMac("80:1f:02:87:82:84");
    if (device_id==-1)
    {
        //the device hasn't been registered yet.
        fprintf(stderr,"Can't find a device on server side with this mac. Creating one...\n");
        cs.addDevice();
    }
    
    //sul lato server creo, e non esistono ancora, i feed corrispondenti ai sensori
    cs.addFeed(11,"raspberry internal temperature");
    cs.addFeed(12,"raspberry internal humidity");
    cs.addFeed(01,"external dust");
    cs.addFeed(02,"external temperature");
    cs.addFeed(03,"external humidity");
        
	while(1)
	{
		cout << CrowdSensing::getCurrentDateUTC() << " Acquisizione mutex" << endl;
        //acquisisco il mutex
		std::unique_lock<std::mutex> ul(mutexLettureDaInviare);
        cout << CrowdSensing::getCurrentDateUTC() << " Attesa condVariable" << endl;

        //rilascia il mutex e attende (senza consumo risorse) che ci siano nuovi dati da inviare
        //quando e' svegliato dalla condizione riacquisice il mutex automaticamente
        ciSonoLettureDaInviare.wait(ul,[&]{return !lettureDaInviare.empty();});
		//copia tutti i dati da inviare nella coda locale, e svuota lettureDaInviare
        codaLocale.push_front( list<SensorReading>(lettureDaInviare) );
        lettureDaInviare.clear();

		//rilascio il mutex
		ul.unlock();

        auto tPrimoInvio = std::chrono::system_clock::now();
        //ritrasmissione finche' la lista non e' vuota o non sono passati 4 minuti
		while (!codaLocale.empty() && ( std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - tPrimoInvio).count() < 4*60*1000) )
		{
	        std::cout << CrowdSensing::getCurrentDateUTC() << " " << codaLocale.size() << " rilevazioni da inviare in codaLocale" << endl;
	        if(cs.inviaRilevazioni(codaLocale.back()))
	        {
	            //inviata con successo
	            codaLocale.pop_back();
	        }
	        //attende un po' prima di ritentare il rinvio
	        std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
		}
        //limitiamo la lunghezza della coda, conserva massimo 3 giorni di rilevazioni
        while (codaLocale.size()>1000)
        {
            codaLocale.pop_back();
        }           
	}
}

