#pragma once
#include <string>

class Sensor
{
public:
	Sensor(int local_feed_id, std::string units);
	~Sensor(void);

	void aggiungiMisura(double misura);
	void reset();

	double average, variance;
	int local_feed_id, n_misure;
    std::string tags;   
    std::string units;
};

