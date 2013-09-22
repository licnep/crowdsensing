#pragma once

#include <cstdlib>
#include <stdio.h>
#include <curl/curl.h>
#include <string>
#include <string.h>

class CurlWrapper
{
public:
    CurlWrapper(std::string username, std::string password);
    ~CurlWrapper();
    std::string sendMessage(int method,std::string APIendpoint,std::string message = std::string(),bool digest_authenticate = false);
    
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
    
    static size_t CurlWriteMemoryCallback( char *ptr, size_t size, size_t nmemb, std::string *userdata);
    
    CURL *curl;
    CURLcode res; //used to store curl results
    
    std::string username, password;
};

