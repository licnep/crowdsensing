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

/**
	Algoritmo numericamente stabile, implementato come da http://www.johndcook.com/standard_deviation.html
	Attenzione perche' calcolata in altri modi possono esserci errori anche gravi
**/
void Sensor::aggiungiMisura(double misura)
{
	n_misure++;

    // See Knuth TAOCP vol 2, 3rd edition, page 232
    if (n_misure == 1)
    {
        m_oldM = m_newM = misura;
        m_oldS = 0.0;
    }
    else
    {
        m_newM = m_oldM + (misura - m_oldM)/n_misure;
        m_newS = m_oldS + (misura - m_oldM)*(misura - m_newM);
    
        // set up for next iteration
        m_oldM = m_newM; 
        m_oldS = m_newS;
    }

}

void Sensor::reset() 
{
	this->n_misure = 0;
}


double Sensor::media()
{
    return (n_misure > 0) ? m_newM : 0.0;
}

double Sensor::varianza()
{
    return ( (n_misure > 1) ? m_newS/(n_misure - 1) : 0.0 );
}