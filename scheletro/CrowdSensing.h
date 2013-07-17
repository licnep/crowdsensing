#pragma once

#include <string>
#include <sstream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/writer.h>
#include <exception>
#include "CurlWrapper.h"
#include "SensorReading.h"
#include <list>

class feed
{
public:
    int local_feed_id;
    std::string tags;
    double average;
    double variance;    
    std::string units;
    std::string lastUpdated; //timestamp of the last update
    std::vector<double> measures;
};

class CrowdSensing
{
public:
    CrowdSensing(std::string  raspb_wifi_mac,std::string  username, std::string  password);
    void setDeployment();
    void checkAPIVersion() ;
    std::string  listRegisteredDevices();
    int getDeviceIDFromMac(std::string  mac_address);
    void getDeviceInfo(std::string  MACaddress);
    void addDevice();
    std::string  listFeeds();
    void addFeed(int local_feed_id, std::string  tags);
    void updateLocalFeed(int local_feed_id, double average, double variance, std::string  units);
    void sensorPost();
    void authorize(std::string  group_id, std::string  password);
    void checkAuthorization(std::string  username);
    std::map<int,feed> get_local_feeds();
    
    int inviaRilevazioni(std::list<SensorReading> &lista);
    
    static std::string  getCurrentDateUTC();
    
private:
    std::string  baseURL; //by default it points to the test API. 
    CurlWrapper cw;
    std::string  raspb_wifi_mac;
    std::string  username;
    std::string  password;
    std::map<int, feed> local_feeds;
};

