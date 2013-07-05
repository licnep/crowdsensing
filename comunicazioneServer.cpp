/* 
 * File:   main.cpp
 * Author: alox
 *
 * Created on May 11, 2013, 6:17 PM
 */

//TODO: maybe substitute "printf" with "cerr <<"
//BUGS da segnalare:
//nelle "Feeds functions" usa /device/ ma dovrebbe essere /deviceS/
//DOMANDE: 
//i tag vanno separati con una virgola?

#include <cstdlib>
#include <curl/curl.h>
#include <string>
#include <string.h>
#include <sstream>
#include <algorithm>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/writer.h>
#include <unistd.h>


using namespace std;

#define DEVICE_ID 200
#define DEVICE_MAC "00:22:fb:8f:9d:fe"

class CurlWrapper
{
public:
    CurlWrapper()
    {
        curl = curl_easy_init();
    }
    ~CurlWrapper()
    {
        curl_easy_cleanup(curl);
    }
    
    /**
     * Generic libcurl wrapper to send a string to the APIendpoint
     * @param method      CurlWrapper::GET or CurlWrapper::POST      
     * @param APIendpoint eg. "/version"
     * @param message     [optional] only for POST requests
     * @return the string returned from the server
     */
    string sendMessage(int method,string APIendpoint,string message = string())
    {        
        if (method!=GET&&method!=POST)
        {
            fprintf(stderr,"ERROR: called sendMessage with a method different from CurlWrapper::GET or CurlWrapper::POST\n");
            fprintf(stderr,"ERROR: sendMessage aborted. Didn't send to '%s'\n",APIendpoint.c_str());
            return "";
        }
        if (!curl)
        {
            fprintf(stderr,"ERROR: curl wasn't properly initialized.\n");
            fprintf(stderr,"ERROR: sendMessage aborted. Didn't send to '%s'\n",APIendpoint.c_str());
            return "";
        }
        
        //create the buffer where we save the result of the request:
        memoryStructForCurl memoryOutput;
        memoryOutput.memory = (char*)malloc(1);
        memoryOutput.size = 0;
        
        curl_easy_reset(curl); //reset all options
        curl_easy_setopt(curl,CURLOPT_URL, APIendpoint.c_str() );
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,CurlWrapper::CurlWriteMemoryCallback);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&memoryOutput); //the extra data to pass to WRITEFUNCTION (it's our allocated memory where we're writing the message bit by bit)

        if (method==POST)
        {
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message.c_str());
            //POST require content-type:application/json (see: http://crowdsensing.ismb.it/SC/rest/application.wadl )
            struct curl_slist *headers=NULL;  
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }
        
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) 
        {
            fprintf(stderr, "ERROR: in sendMessage curl_easy_perform() failed: %s1n",curl_easy_strerror(res));
            return "";
        }
        
        string result(memoryOutput.memory); //copy output in result string
        free(memoryOutput.memory);
        return result;
    }
    
    void digestAuthenticate(string username, string password,string APIendpoint)
    {
        
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,NULL); //print output to screen, default behaviour
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,stderr);
        curl_easy_setopt(curl, CURLOPT_URL, APIendpoint.c_str());
        curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC|CURLAUTH_DIGEST);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) 
        {
            fprintf(stderr, "in digestAuthenticate curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
            return;
        }
        //stays authenticated until curl_easy_cleanup
    }
    
    enum Method {
        GET,
        POST
    };
    
private:
    struct memoryStructForCurl
    {
        char *memory;
        size_t size;
    };
    
    /**
     * This static method is called by curl every time it gets a new chunk of data, and it appends it to the userdata
     * @param ptr contains the new data to write
     * @param size sizeof
     * @param nmemb how many (multiply by size to get real size)
     * @param userdata must be of memoryStructForCurl type. Normally a CrowdSensing object passes its own memoryOutput;
     * @return 
     */
    static size_t CurlWriteMemoryCallback( char *ptr, size_t size, size_t nmemb, memoryStructForCurl *userdata)
    {
        size_t realsize = size * nmemb;
        memoryStructForCurl *mem = (memoryStructForCurl*)userdata;
        
        mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
        if (mem->memory == NULL)
        {
            fprintf(stderr, "ERROR: out of memory! Cannot allocate enough memory to write the curl output.\n");
            return 0;
        }
        memcpy(&(mem->memory[mem->size]),ptr, realsize);
        mem->size += realsize;
        mem->memory[mem->size] = 0; //null termination
        return realsize;
    }
    
    CURL *curl;
    CURLcode res; //used to store curl results
};

class feed
{
public:
    int local_feed_id;
    string tags;
    double average;
    double variance;    
    string units;
    string lastUpdated; //timestamp of the last update
    vector<double> measures;
};

class CrowdSensing
{
public:
    /**
     * By default it uses the test API endpoint. Call setDeployment() to use the deployment endpoint
     */
    CrowdSensing(string raspb_wifi_mac,string username, string password) : cw() //initialize the curl wrapper
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
    
    void setDeployment()
    {
        //baseURL = "http://crowdsensing.ismb.it/SC/rest/apis";
    }
    
    void checkAPIVersion() 
    {
        string result = cw.sendMessage(CurlWrapper::GET,baseURL + "/version");
        printf("[API version]: %s\n",result.c_str());
        if(result.compare("0.4.9-test")) 
        {
            fprintf(stderr,"WARNING: API current version is %s, this program was written for 0.4.9-test, something important may have changed\n",result.c_str());
        }
    }
    
    string listRegisteredDevices() 
    {
        //TODO stub
        string result = cw.sendMessage(CurlWrapper::GET,baseURL + "/devices");
        return result;
    }
    
    /**
     * Gets the list of devices from the server and looks for one with the corresponding mac address
     * @param mac_address
     * @return -1 if not found, id if found
     */
    int getDeviceIDFromMac(string mac_address)
    {
        string devices = listRegisteredDevices();
        
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
                string mac;
                try
                {
                    mac = root[i]["raspb_wifi_mac"].asString();
                }
                catch(exception e) 
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
                    catch(exception e) 
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
    
    void getDeviceInfo(string MACaddress) 
    {
        //TODO stub
        string result = cw.sendMessage(CurlWrapper::GET,baseURL + string("/devices/")+ MACaddress);
        printf("[Device Info]:\n%s",result.c_str());
    }
    
    void addDevice()
    {
        string json = "{\"username\":\""+username+"\",\"raspb_wifi_mac\":\""+raspb_wifi_mac+"\"}";
        string result = cw.sendMessage(CurlWrapper::POST,baseURL + string("/devices"),json);
        printf("[Add Device]: %s\n",result.c_str());
        
        //if a device with this mac already exists, returns "InvalidPostException: Posting new Device with [mac=00:11:22:33:9d:fe] but it is already there ! - use put to modify"
    }
    
    string listFeeds()
    {
        string result = cw.sendMessage(CurlWrapper::GET,baseURL + "/devices/"+raspb_wifi_mac+"/feeds");
        printf("[List Feeds]:%s\n",result.c_str());
        return result;
        //returns "Missing resource: Resource not found: No feeds have been entered for [mac=b8:27:eb:69:a4:20]" if cant find any feed
    }
    
    /**
     * Add feeds locally and remotely if they don't yet exist
     * @param local_feed_id
     * @param tags comma separated tags
     */
    void addFeed(int local_feed_id, string tags = "")
    {
        local_feeds[local_feed_id].local_feed_id = local_feed_id;
        local_feeds[local_feed_id].tags = tags;
        local_feeds[local_feed_id].lastUpdated = getCurrentDateUTC();
        //update feeds remotely
        stringstream json;
        json << "{\"tags\":\"" << tags << "\",\"local_feed_id\":" << local_feed_id << "}";
        string result = cw.sendMessage(CurlWrapper::POST,baseURL + "/devices/"+raspb_wifi_mac+"/feeds",json.str());
        printf("[Add Feed]: %s\n",result.c_str());
    }
    
    void addMeasureToAverage(int local_feed_id, double measure)
    {
        local_feeds[local_feed_id].measures.push_back(measure);
    }
    
    void calculateAvgAndVariance(int local_feed_id)
    {
        double avg = 0;
        vector<double> *m = &local_feeds[local_feed_id].measures;
        for (vector<double>::iterator i=m->begin();i!=m->end();i++)
        {
            avg += *i;
        }
        avg=avg/m->size();
        printf("Average: %f",avg);
        double var = 0;
        for (vector<double>::iterator i=m->begin();i!=m->end();i++)
        {
            var += (*i-avg)*(*i-avg);
        }
        var = var/(m->size()-1);
        printf("Variance: %f",var);
    }
    
    string getCurrentDateUTC()
    {
        char buffer[200];
        time_t now = time(NULL);
        //example: 2013-05-28T18:00:06.021+0200  (gmtime converts to UTC (+0000))
        strftime(buffer,sizeof(char)*199,"%Y-%m-%dT%H:%M:%S%z",gmtime(&now));
        return string(buffer);
    }
    
    void updateLocalFeed(int local_feed_id, double average, double variance, string units)
    {
        local_feeds[local_feed_id].average = average;
        local_feeds[local_feed_id].variance = variance;
        local_feeds[local_feed_id].units = units;
        local_feeds[local_feed_id].lastUpdated = getCurrentDateUTC();
    }
    
    void sensorPost()
    {
        //see http://crowdsensing.ismb.it/SC/rest/examples/sensor_post.html  for the format
        Json::StyledWriter writer;
        Json::Value root; //root json element
        root["send_timestamp"] = getCurrentDateUTC();
        //populate the "sensor_values" array with all our feeds
        for (map<int,feed>::iterator i = local_feeds.begin(); i!=local_feeds.end();i++)
        {
            //calculate the average and variance
            calculateAvgAndVariance(i->second.local_feed_id);
            
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
        stringstream json;
        json << writer.write(root);
        
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
        string result = cw.sendMessage(CurlWrapper::POST,baseURL + "/device/" +raspb_wifi_mac + "/posts",json.str().c_str());
        printf("[Sensor Post]:%s\n",result.c_str());
    }
    
    void authorize(string group_id, string password)
    {
        cw.digestAuthenticate(group_id,password,baseURL + "/auth/"+group_id);
    }
    
    void checkAuthorization(string username)
    {
        string result = cw.sendMessage(CurlWrapper::GET,baseURL + "/auth/"+ username);
        printf("\nAUTHORIZATION:\n%s",result.c_str());
    }
    
    map<int,feed> get_local_feeds() 
    {
        return local_feeds;
    }
    
private:
    string baseURL; //by default it points to the test API. 
    CurlWrapper cw;
    string raspb_wifi_mac;
    string username;
    string password;
    map<int, feed> local_feeds;
};

/*
 * 
 */
int main(int argc, char** argv) 
{
   
    CrowdSensing cs("b8:27:eb:69:a4:20","gruppo35","password");
    
    
    //add our feeds
    cs.addFeed(1,"raspberry, internal, temperature");
    cs.addFeed(2,"raspberry, internal, humidity");
    cs.addFeed(3,"external, temperature");
    cs.addFeed(4,"external, humidity");
    cs.addFeed(5,"external, dust");

    //1,3,5,4,5,2,1,1,3,2.
    cs.addMeasureToAverage(1,1);
    cs.addMeasureToAverage(1,3);
    cs.addMeasureToAverage(1,5);
    cs.addMeasureToAverage(1,4);
    cs.addMeasureToAverage(1,5);
    cs.addMeasureToAverage(1,2);
    cs.addMeasureToAverage(1,1);
    cs.addMeasureToAverage(1,1);
    cs.addMeasureToAverage(1,3);
    cs.addMeasureToAverage(1,2);
    cs.calculateAvgAndVariance(1);
    /*int i=30;
    while (i<100) {
        cs.updateLocalFeed(1,10.00000,0,"Degrees C");
        cs.updateLocalFeed(2,20.00000,5,"% humidity");
        cs.updateLocalFeed(3,33.03030303,4.4400111,"Degrees C");
        cs.updateLocalFeed(4,40.00000,1.111111,"% humidity");
        cs.updateLocalFeed(5,i++,1.111111,"pm");
        cs.sensorPost();
        sleep(1);
    }*/
    
    //cs.getDeviceInfo("b8:27:eb:69:a4:19");
    //cs.authorize("gruppo35","034FpK69l4");
    //cs.checkAuthorization("gruppo35");
    //int device_id = cs.getDeviceIDFromMac("b8:27:eb:69:a4:20");
    //printf("Il mio device id e':%d\n",device_id);
    //cs.addFeed(998,"frigorifero");
    //cs.listFeeds();
    //cs.sensorPost();
    return 0;
}


