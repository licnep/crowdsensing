#include <list>

#include "CrowdSensing.h"
#include <chrono>

/**
 * By default it uses the test API endpoint. Call setDeployment() to use the deployment endpoint
 */
CrowdSensing::CrowdSensing(std::string  raspb_wifi_mac,std::string  username, std::string  password) : cw() //initialize the curl wrapper
{
    this->raspb_wifi_mac = raspb_wifi_mac;
    this->username = username;
    this->password = password;
    baseURL = "http://crowdsensing.ismb.it/SC/rest/test-apis";

    //return a warning if API version has been changed
    checkAPIVersion();

    //try to register this device if it isn't registered already
    int device_id = getDeviceIDFromMac(this->raspb_wifi_mac);
    if (device_id==-1)
    {
        fprintf(stderr,"Can't find a device on server side with mac:%s - Creating one...\n",raspb_wifi_mac.c_str());
        //the device hasn't been registered yet.
        addDevice();
    }
}

void CrowdSensing::setDeployment()
{
    baseURL = "http://crowdsensing.ismb.it/SC/rest/apis";
}

void CrowdSensing::checkAPIVersion() 
{
    std::string  result = cw.sendMessage(CurlWrapper::GET,baseURL + "/version");
    printf("[API version]: %s\n",result.c_str());
    if(result.compare("0.4.9-test")) 
    {
        fprintf(stderr,"WARNING: API current version is %s, this program was written for 0.4.9-test, something important may have changed\n",result.c_str());
    }
}

std::string  CrowdSensing::listRegisteredDevices() 
{
    //TODO stub
    std::string  result = cw.sendMessage(CurlWrapper::GET,baseURL + "/devices");
    return result;
}

/**
 * Gets the list of devices from the server and looks for one with the corresponding mac address
 * @param mac_address
 * @return -1 if not found, id if found
 */
int CrowdSensing::getDeviceIDFromMac(std::string  mac_address)
{
    std::string  devices = listRegisteredDevices();

    Json::Value root;   // will contains the root value after parsing.
    Json::Reader reader;
    bool parsingSuccessful = reader.parse( devices, root );
    if ( !parsingSuccessful )
    {
        fprintf(stderr,"Failed to parse device list\n%s",devices.c_str());
        return -1;
    }
    //look for a device with this mac address
    if(root.isArray())
    {
        for(int i=0;i<root.size();i++)
        {
            std::string  mac;
            try
            {
                mac = root[i]["raspb_wifi_mac"].asString();
            }
            catch(std::exception e) 
            {
                printf("Exception while parsing device list.\n");
                return -1;
            }
            if (mac==mac_address)
            {
                int id;
                try
                {
                    id = root[i]["id"].asInt();
                }
                catch(std::exception e) 
                {
                    printf("Exception while parsing device list (device_id).\n");
                    return -1;
                }
                return id;
            }
        }
    }
    return -1; //not found
}

void CrowdSensing::getDeviceInfo(std::string  MACaddress) 
{
    //TODO stub
    std::string  result = cw.sendMessage(CurlWrapper::GET,baseURL + std::string("/devices/")+ MACaddress);
    printf("[Device Info]:\n%s",result.c_str());
}

void CrowdSensing::addDevice()
{
    std::string  json = "{\"username\":\""+username+"\",\"raspb_wifi_mac\":\""+raspb_wifi_mac+"\"}";
    std::string  result = cw.sendMessage(CurlWrapper::POST,baseURL + std::string("/devices"),json);
    printf("[Add Device]: %s\n",result.c_str());

    //if a device with this mac already exists, returns "InvalidPostException: Posting new Device with [mac=00:11:22:33:9d:fe] but it is already there ! - use put to modify"
}

std::string  CrowdSensing::listFeeds()
{
    std::string  result = cw.sendMessage(CurlWrapper::GET,baseURL + "/devices/"+raspb_wifi_mac+"/feeds");
    printf("[List Feeds]:%s\n",result.c_str());
    return result;
    //returns "Missing resource: Resource not found: No feeds have been entered for [mac=b8:27:eb:69:a4:20]" if cant find any feed
}

/**
 * Add feeds locally and remotely if they don't yet exist
 * @param local_feed_id
 * @param tags comma separated tags
 */
void CrowdSensing::addFeed(int local_feed_id, std::string  tags = "")
{
    local_feeds[local_feed_id].local_feed_id = local_feed_id;
    local_feeds[local_feed_id].tags = tags;
    local_feeds[local_feed_id].lastUpdated = getCurrentDateUTC();
    //update feeds remotely
    std::stringstream json;
    json << "{\"tags\":\"" << tags << "\",\"local_feed_id\":" << local_feed_id << "}";
    std::string  result = cw.sendMessage(CurlWrapper::POST,baseURL + "/devices/"+raspb_wifi_mac+"/feeds",json.str());
    printf("[Add Feed]: %s\n",result.c_str());
}

std::string  CrowdSensing::getCurrentDateUTC()
{
    char buffer[200];
    time_t now = time(NULL);
    //example: 2013-05-28T18:00:06.021+0200  (gmtime converts to UTC (+0000))
    strftime(buffer,sizeof(char)*199,"%Y-%m-%dT%H:%M:%S%z",gmtime(&now));
    return std::string(buffer);
}

void CrowdSensing::updateLocalFeed(int local_feed_id, double average, double variance, std::string  units)
{
    local_feeds[local_feed_id].average = average;
    local_feeds[local_feed_id].variance = variance;
    local_feeds[local_feed_id].units = units;
    local_feeds[local_feed_id].lastUpdated = getCurrentDateUTC();
}

void CrowdSensing::sensorPost()
{
    //see http://crowdsensing.ismb.it/SC/rest/examples/sensor_post.html  for the format
    Json::StyledWriter writer;
    Json::Value root; //root json element
    root["send_timestamp"] = getCurrentDateUTC();
    //populate the "sensor_values" array with all our feeds
    for (std::map<int,feed>::iterator i = local_feeds.begin(); i!=local_feeds.end();i++)
    {
        //calculate the average and variance
        //calculateAvgAndVariance(i->second.local_feed_id);

        Json::Value sensor_values;
        feed *f = &i->second;
        sensor_values["value_timestamp"] = f->lastUpdated;
        sensor_values["average_value"] = f->average;
        sensor_values["local_feed_id"] = f->local_feed_id;
        sensor_values["variance"] = f->variance;
        sensor_values["units_of_measurement"] = f->units;
        root["sensor_values"].append(sensor_values);
    }
    //cout << writer.write(root);
    std::stringstream json;
    json << writer.write(root);
    
    std::cout << json.str();
    return;

    /*json << 
            "{"
                "\"send_timestamp\": \"" << getCurrentDateUTC() << "\"," //e' necessario
//                    "\"position\": {" //must be hardcoded
//                        "\"kind\": \"latitude#location\","
//                        "\"timestampMs\": \"1274057512199\","
//                        "\"latitude\": 37.3860517,"
//                        "\"longitude\": -122.0838511,"
//                        "\"accuracy\": 5000,"
//                        "\"height_meters\": 0"
//                    "},"
                "\"sensor_values\": [";

    for (map<int,feed>::iterator i = local_feeds.begin(); i!=local_feeds.end();i++)
    {
        feed f = i->second;
        json << "{"
            "\"value_timestamp\": \"" << f.lastUpdated << "\","
                "\"average_value\": " << f.average << ","
                "\"local_feed_id\": " << f.local_feed_id << ","
                "\"variance\": " << f.variance << ","
                "\"units_of_measurement\": \"" << f.units << "\""
            "},";
    }
    json << "]}";
    cout << json.str();
    */
    std::string  result = cw.sendMessage(CurlWrapper::POST,baseURL + "/device/" +raspb_wifi_mac + "/posts",json.str().c_str());
    printf("[Sensor Post]:%s\n",result.c_str());
}

/**
 * Prova a inviare la lista di rilevazioni, se ha successo svuota la lista
 * @param lista
 */
int CrowdSensing::inviaRilevazioni(std::list<SensorReading> &lista)
{
    Json::StyledWriter writer;
    Json::Value root; //root json element
    root["send_timestamp"] = getCurrentDateUTC();
    
    //per latitudine e longitudine usato http://diveintohtml5.info/geolocation.html
    Json::Value position;
    position["kind"] = "latitude#location";
    position["timestampMs"] = "1374105807337";
    position["latitude"] = (double)45.4626922; //ivrea, via miniere
    position["longitude"] = (double)7.87265;
    position["accuracy"] = 71;
    position["height_meters"] = 0;
    root["position"] = position;
    
    //populate the "sensor_values" array with all our feeds
    for ( std::list<SensorReading>::iterator i = lista.begin(); i!=lista.end(); i++ )
    {
        Json::Value sensor_values;
        sensor_values["value_timestamp"] = i->timestamp;
        sensor_values["average_value"] = i->average;
        sensor_values["local_feed_id"] = i->local_feed_id;
        sensor_values["variance"] = i->variance;
        sensor_values["units_of_measurement"] = i->units;
        root["sensor_values"].append(sensor_values);
    }
    //cout << writer.write(root);
    std::stringstream json;
    json << writer.write(root);
    
    // std::cout << json.str();
    
    std::string  result = cw.sendMessage(CurlWrapper::POST,baseURL + "/device/" +raspb_wifi_mac + "/posts",json.str().c_str());
    //printf("[Sensor Post]:%s\n",result.c_str());
    
    //se inviato con successo svuoto la lista
    if (result.compare("")!=0) 
    {
        std::cout << getCurrentDateUTC() << "RISPOSTA: " << result.c_str() << std::endl;
        //se tutto e' andato bene il server risponde con lo stesso json che gli e' stato inviato
        //facciamo il parsing dello json per verificare
        Json::Value rootReply;   // will contains the root value after parsing.
        Json::Reader reader;
        bool parsingSuccessful = reader.parse( result, rootReply );
        if ( !parsingSuccessful )
        {       
            // report to the user the failure and their locations in the document.
            std::cout << getCurrentDateUTC() << " Failed to parse json response: \n" << reader.getFormattedErrorMessages();
            return 0;
        } 
        else {
            if (rootReply["raspb_wifi_mac"].asString().compare(raspb_wifi_mac)==0) 
            {
                std::cout << "il mac della risposta corrisponde!!!!\n" ;
                lista.clear();
                return 1;
            } 
            else {
                std::cout << "mac DIVERSI :(\n" ;
                return 0;
            }
        }
    } else {
        std::cout << CrowdSensing::getCurrentDateUTC() << " Errore nell'invio, riprovo dopo." << std::endl;
    }
    return 0;
}

void CrowdSensing::authorize(std::string  group_id, std::string  password)
{
    cw.digestAuthenticate(group_id,password,baseURL + "/auth/"+group_id);
}

void CrowdSensing::checkAuthorization(std::string  username)
{
    std::string  result = cw.sendMessage(CurlWrapper::GET,baseURL + "/auth/"+ username);
    printf("\nAUTHORIZATION:\n%s",result.c_str());
}

std::map<int,feed> CrowdSensing::get_local_feeds() 
{
    return local_feeds;
}

