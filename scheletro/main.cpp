#include <iostream>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "Sensor.h"
#include "SensorReading.h"

#define SIMULATION true

void threadComunicazioneServer(); //definito piu' in basso

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

	std::chrono::system_clock::time_point ultimoSalvataggio = std::chrono::system_clock::now();
	while(1) //while dell'USB (ogni 8ms)
	{
		if(SIMULATION) std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
		
		//TODO: leggi tutti i sensori
		polveri.aggiungiMisura(5);
		temp.aggiungiMisura(5);
		umidita.aggiungiMisura(5);
		temp_rasp.aggiungiMisura(5);
		umidita_rasp.aggiungiMisura(5);

		auto millisecondiDaUltimoSalvataggio = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - ultimoSalvataggio).count();
		if (millisecondiDaUltimoSalvataggio > 1000) //5*60*1000=5 minuti
		{
			cout << "passati 30 secondi" << endl;
			ultimoSalvataggio = std::chrono::system_clock::now();
			{
				//ottengo il mutex per scrivere nella coda
				std::lock_guard<std::mutex> l(mutexLettureDaInviare);
				//TODO: forse si potrebbe usare try_lock_for() per evitare di perdere la sincronizzazione usb
				//aggiunge tutti i valori alla coda
				lettureDaInviare.push_front(SensorReading(polveri));
				lettureDaInviare.push_front(SensorReading(temp));
				lettureDaInviare.push_front(SensorReading(umidita));
				lettureDaInviare.push_front(SensorReading(temp_rasp));
				lettureDaInviare.push_front(SensorReading(umidita_rasp));
			}
			
			//notifico che c'e' roba da inviare in coda:
			ciSonoLettureDaInviare.notify_one();

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
	list<SensorReading> listaLocale;
	while(1)
	{
		{
			//attende senza consumo risorse e vieni risvegliato solo se la condition variable della coda ti notifica
			std::unique_lock<std::mutex> ul(mutexLettureDaInviare);
			ciSonoLettureDaInviare.wait(ul);

			if (!lettureDaInviare.empty())
			{
				//copia tutti i dati da inviare nella coda locale, e svuota lettureDaInviare
				listaLocale.splice(listaLocale.begin(),lettureDaInviare);
			}
		} //rilascio il mutex

		if (!listaLocale.empty())
		{
			//TODO: prova a inviare al server tutti i rilevamenti contenuti nella listaLocale
		}
	}
}