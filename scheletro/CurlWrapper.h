#pragma once

#include <stdio.h>
#include <curl/curl.h>
#include <string>
#include <string.h>
/*#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/writer.h>
#include <unistd.h>
*/

class CurlWrapper
{
public:
    CurlWrapper();
    ~CurlWrapper();
    std::string sendMessage(int method,std::string APIendpoint,std::string message = std::string());
    
    void digestAuthenticate(std::string username, std::string password,std::string APIendpoint);
    
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
    
    static size_t CurlWriteMemoryCallback( char *ptr, size_t size, size_t nmemb, memoryStructForCurl *userdata);
    
    CURL *curl;
    CURLcode res; //used to store curl results
};


