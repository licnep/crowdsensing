#pragma once
#include <string>

class Sensor
{
public:
	Sensor(int local_feed_id, std::string units);
	~Sensor(void);

	void aggiungiMisura(double misura);
	void reset();
	double media();
	double varianza();

	int local_feed_id;
	std::string tags;   
    std::string units;

private:
	int n_misure;
	double m_oldM, m_newM, m_oldS, m_newS;
};

