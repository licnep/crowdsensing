#include "Sensor.h"


Sensor::Sensor(int local_feed_id, std::string units)
{
	this->local_feed_id = local_feed_id;
	this->units = units;
	this->reset();
}


Sensor::~Sensor(void)
{
}

void Sensor::reset() 
{
	this->n_misure = 0;
	this->average = 0;
	this->variance = 0;
}