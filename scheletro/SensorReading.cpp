#include "SensorReading.h"


SensorReading::SensorReading(Sensor s)
{
	//copia la lettura dal sensore
	this->average = s.average;
	this->variance = s.variance;
	this->local_feed_id = s.local_feed_id;
	this->tags = s.tags;
	this->units = s.units;
	this->timestamp = "TODO: TIMESTAMP"; //TODO ottenere la stringa del timestamp attuale
}


SensorReading::~SensorReading(void)
{
}
