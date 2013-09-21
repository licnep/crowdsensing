#pragma once

#include <string>
#include <sstream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/writer.h>
#include <exception>
#include "CurlWrapper.h"
#include "SensorReading.h"
#include <list>
#include <iwlib.h>

struct apinfo_s{
    iwrange range;
    wireless_scan_head head; 		
};

typedef struct apinfo_s apinfo;

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
    CrowdSensing(std::string  raspb_wifi_mac,std::string  username, std::string  password,bool deployment=false);
    void setDeployment();
    void checkAPIVersion() ;
    std::string  listRegisteredDevices();
    int getDeviceIDFromMac(std::string  mac_address);
    void getDeviceInfo(std::string  MACaddress);
    void addDevice();
    std::string  listFeeds();
    void addFeed(int local_feed_id, std::string  tags);
    std::map<int,feed> get_local_feeds();
    
    int inviaRilevazioni(std::list<SensorReading> &lista);
    
    static std::string  getCurrentDateUTC();
    
private:
    apinfo getAPList();
    void getLocation();


    std::string  baseURL; //by default it points to the test API. 
    CurlWrapper cw;
    std::string  raspb_wifi_mac;
    std::string  username;
    std::string  password;
    std::map<int, feed> local_feeds;
	Json::Value position;
};

