#pragma once

#include <string>
#include "Sensor.h"

class SensorReading
{
public:
	SensorReading(Sensor s);
	~SensorReading(void);

private:
	double average, variance;
	int local_feed_id;
    std::string tags;   
    std::string units;
    std::string timestamp; //timestamp of the last update
};

