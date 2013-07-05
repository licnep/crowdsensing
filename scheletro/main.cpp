#include <iostream>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "Sensor.h"
#include "SensorReading.h"

using namespace std;

//GLOBAL: (la coda dei dati da inviare, condivisa fra i due thread)
list<SensorReading> lettureDaInviare;
std::mutex mutexLettureDaInviare;
std::condition_variable ciSonoLettureDaInviare;

int main(int argc, char* argv[]) {
	Sensor polveri(01,"pm"), temp(02,"Celsius"), umidita(03,"%"), temp_rasp(11,"Celsius"), umidita_rasp(12,"%");
	
	//TODO: Inizializza USB e I2C

	//avvio il thread di comunicazione col server
	std::thread thread2(threadComunicazioneServer);

	while(1) //while dell'USB ogni 8ms
	{
		//TODO: leggi tutti i sensori e per ognuno chima es. polveri.aggiungiMisura(misura);

		if (1/*TODO sono passati 5 minuti*/)
		{
			//ottengo il mutex per scrivere nella coda
			std::unique_lock<std::mutex> ul(mutexLettureDaInviare);
			//aggiunge tutti i valori alla coda
			lettureDaInviare.push_front(SensorReading(polveri));
			lettureDaInviare.push_front(SensorReading(temp));
			lettureDaInviare.push_front(SensorReading(umidita));
			lettureDaInviare.push_front(SensorReading(temp_rasp));
			lettureDaInviare.push_front(SensorReading(umidita_rasp));

			//notifico che c'e' roba da inviare in coda:
			ciSonoLettureDaInviare.notify_one();

			//rilascia mutex coda
			ul.unlock();

			//resetto i conteggi per media e varianza
			polveri.reset();
			temp.reset();
			umidita.reset();
			temp_rasp.reset();
			umidita_rasp.reset();
		}
	}

	thread2.join();
	return 0;
}


void threadComunicazioneServer()
{
	while(1)
	{
		//attendi senza consumo risorse e vieni risvegliato solo se la condition variable della coda ti notifica
		std::unique_lock<std::mutex> ul(mutexLettureDaInviare);
		ciSonoLettureDaInviare.wait(ul);
		//TODO: qui controlla se c'e' roba in coda e se si' la invia
	}
}