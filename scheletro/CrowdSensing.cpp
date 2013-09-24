#include <list>
#include <chrono>
#include <iwlib.h>
#include <stdexcept>
#include "CrowdSensing.h"

/**
 * By default it uses the test API endpoint. set deployment to true to use the deployment endpoint
 */
CrowdSensing::CrowdSensing(std::string  raspb_wifi_mac,std::string  username, std::string  password,bool deployment) : cw(username,password) //initialize the curl wrapper
{
    this->raspb_wifi_mac = raspb_wifi_mac;
    this->username = username;
    this->password = password;
    if (deployment) baseURL = baseURL = "http://crowdsensing.ismb.it/SC/rest/apis";
    else baseURL = "http://crowdsensing.ismb.it/SC/rest/test-apis";
}

apinfo CrowdSensing::getAPList(){
    apinfo info;
    int sock;

    /* Open socket to kernel */
    sock = iw_sockets_open();

    std::string interface = "wlan0";

    /* Get some metadata to use for scanning */
    if (iw_get_range_info(sock, interface.c_str(), &(info.range)) < 0) {
        //TO DO: qui fare un blocco try-catch e gestire l'errore
        std::cerr << "Error during iw_get_range_info. Aborting.\n";
        exit(2);
    }

  /* Perform the scan */
    if (iw_scan(sock, const_cast<char*>(interface.c_str()), info.range.we_version_compiled, &(info.head)) < 0) {
        std::cerr << "Error during iw_scan. Aborting.\n";
        exit(2);
    }

    return info;
}

void CrowdSensing::getLocation(){

    int level=0;
    char buffer[128];
    std::stringstream json;    
    apinfo info=getAPList();
    wireless_scan *result=info.head.result;

    json << "{\"wifiAccessPoints\": [";
    while (NULL != result) {
    	
        level = result->stats.qual.level;        
        if(level >= 64){
            level -= 0x100;
        }

         json <<   "{\"macAddress\":\"" << iw_sawap_ntop(&result->ap_addr, buffer) << "\"," //required
                    "\"signalStrength\":"  << level << ", " 
                    "\"age\":0,"
                    "\"channel\":"<< iw_freq_to_channel(result->b.freq,&(info.range))  << ","
                    "\"signalToNoiseRatio\":" << (int)result->stats.qual.qual  << "}";	
        //printf("%s, \t %d dBm \t %s \n", result->b.essid, level, buffer);

        result = result->next;
        if(result !=NULL)
        json << ",";
    }

    json << "]}";
    std::cout << json.str() << "\n\n";

    //std::string  sresult = cw.sendMessage(CurlWrapper::POST,baseURL + "/device/" +raspb_wifi_mac + "/geolocate",json.str(), true);
    //std::string  sresult = cw.sendMessage(CurlWrapper::POST,"https://www.googleapis.com/geolocation/v1/geolocate?key=AIzaSyC8i79TqtQm9gAbFngp4TsfH7JLr6NMOLE",json.str(), true);
    std::string  sresult = cw.sendMessage(CurlWrapper::POST,"http://jonjonson.com/crowdsensing/proxy.php?url=https://www.googleapis.com/geolocation/v1/geolocate?key=AIzaSyBlab35Z_3d-2p41w2uhWfzwPpLPi0zjlY",json.str(), true);

    std::cout << "[Risposta API geolocalizzazione]:" << sresult.c_str() << std::endl;

    Json::Reader reader;
    Json::Value root;
    bool parsingSuccessful = reader.parse( sresult, root );
    if ( parsingSuccessful )
    {
        this->position["kind"] = "latitude#location";
        try
        {
            /*
            this->position["timestampMs"] = root["timestampMs"].asString();
            this->position["latitude"] = root["latitude"].asDouble();
            this->position["longitude"] = root["longitude"].asDouble();
            this->position["accuracy"] = root["accuracy"].asInt();       
            this->position["height_meters"] = 0;*/

            //la risposta dell'api google e' diversa quindi il parsing da fare e' leggermente diverso:
            this->position["timestampMs"] = root["timestampMs"].asString();
            this->position["latitude"] = root["location"]["lat"].asDouble();
            this->position["longitude"] = root["location"]["lng"].asDouble();
            this->position["accuracy"] = root["accuracy"].asDouble();
            this->position["height_meters"] = 0;
            std::cout << this->position;

            if (position["accuracy"]==0.0) 
            {
                throw std::runtime_error("Accuracy=0, errore nella geolocalizzazione");   
            }

            //non ci sono stati errori, return. Altrimenti fallback verso la location scritta a mano piu' in basso
            return;
        }
        catch(std::exception& e) 
        {
            std::cerr << "Exception while parsing location.\nMessage: " << e.what() << std::endl;
        }
    }

    std::cerr << "Using hardcoded location.\n";
    
    //per latitudine e longitudine usato http://diveintohtml5.info/geolocation.html
    this->position["kind"] = "latitude#location";
    this->position["timestampMs"] = "1374105807337";
    this->position["latitude"] = (double)45.4626922; //ivrea, via miniere
    this->position["longitude"] = (double)7.87265;
    this->position["accuracy"] = 71;
    this->position["height_meters"] = 0;

}



/**
 * Prova a inviare la lista di rilevazioni, se ha successo svuota la lista
 * @param lista
 */
int CrowdSensing::inviaRilevazioni(std::list<SensorReading> &lista)
{
    Json::StyledWriter writer;
    Json::Value root;
    root["send_timestamp"] = getCurrentDateUTC();
    root["position"] = this->position;
    
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
    std::stringstream json;
    json << writer.write(root);
    std::string  result = cw.sendMessage(CurlWrapper::POST,baseURL + "/device/" +raspb_wifi_mac + "/posts",json.str().c_str(),true);
    
    //se inviato con successo svuoto la lista
    if (result.compare("")!=0) 
    {
        std::cout << getCurrentDateUTC() << "RISPOSTA: " << result.c_str() << std::endl;
        //se tutto e' andato bene il server risponde con lo stesso json che gli e' stato inviato
        //facciamo il parsing per verificare
        Json::Value rootReply;
        Json::Reader reader;
        bool parsingSuccessful = reader.parse( result, rootReply );
        if ( !parsingSuccessful )
        {       
            // report to the user the failure and their locations in the document.
            std::cerr << getCurrentDateUTC() << " Failed to parse json response: \n" << reader.getFormattedErrorMessages();
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
                std::cerr << "mac DIVERSI :(\n" ;
                return 0;
            }
        }
    } else {
        std::cerr << CrowdSensing::getCurrentDateUTC() << " Errore nell'invio, riprovo dopo." << std::endl;
    }
    return 0;
}


void CrowdSensing::checkAPIVersion() 
{
    std::string  result = cw.sendMessage(CurlWrapper::GET,baseURL + "/version");
    printf("[API version]: %s\n",result.c_str());
    if(result.compare("0.4.9-test")&&result.compare("0.4.9"))
    {
        std::cerr << "WARNING: API current version is " << result.c_str() << ", this program was written for 0.4.9-test, something important may have changed\n";
    }
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
        std::cerr << "Failed to parse device list" << devices.c_str() << std::endl;
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
                std::cerr << "Exception while parsing device list.\n";
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
                    std::cerr << "Exception while parsing device list (device_id).\n";
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
    std::string  result = cw.sendMessage(CurlWrapper::GET,baseURL + std::string("/devices/")+ MACaddress);
    std::cout << "[Device Info]:\n" << result.c_str() << std::endl;
}

void CrowdSensing::addDevice()
{
    std::string  json = "{\"username\":\""+username+"\",\"raspb_wifi_mac\":\""+raspb_wifi_mac+"\"}";
    std::string  result = cw.sendMessage(CurlWrapper::POST,baseURL + std::string("/devices"),json,true);
    std::cout << "[Add Device]: %s" << result.c_str() << std::endl;
    //if a device with this mac already exists, returns "InvalidPostException: Posting new Device with [mac=00:11:22:33:9d:fe] but it is already there ! - use put to modify"
}

std::string  CrowdSensing::listFeeds()
{
    std::string  result = cw.sendMessage(CurlWrapper::GET,baseURL + "/devices/"+raspb_wifi_mac+"/feeds");
    std::cout << "[List Feeds]:%s" << result.c_str() << std::endl;
    return result;
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
    std::string  result = cw.sendMessage(CurlWrapper::POST,baseURL + "/devices/"+raspb_wifi_mac+"/feeds",json.str(),true);
    std::cout << "[Add Feed]: " << result.c_str() << std::endl;
}

std::string  CrowdSensing::listRegisteredDevices() 
{
    std::string  result = cw.sendMessage(CurlWrapper::GET,baseURL + "/devices");
    return result;
}

std::string  CrowdSensing::getCurrentDateUTC()
{
    char buffer[200];
    time_t now = time(NULL);
    //example: 2013-05-28T18:00:06.021+0200  (gmtime converts to UTC (+0000))
    strftime(buffer,sizeof(char)*199,"%Y-%m-%dT%H:%M:%S%z",gmtime(&now));
    return std::string(buffer);
}

std::map<int,feed> CrowdSensing::get_local_feeds() 
{
    return local_feeds;
}

