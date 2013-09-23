#include "CurlWrapper.h"


CurlWrapper::CurlWrapper(std::string username, std::string password)
{
    this->username = username;
    this->password = password;
    curl = curl_easy_init();
}


CurlWrapper::~CurlWrapper(void)
{
	curl_easy_cleanup(curl);
}
    
/**
    * Generic libcurl wrapper to send a string to the APIendpoint
    * @param method      CurlWrapper::GET or CurlWrapper::POST      
    * @param APIendpoint eg. "http://....../version"
    * @param message     [optional] only for POST requests
    * @return the string returned from the server
    */
std::string CurlWrapper::sendMessage(int method,std::string APIendpoint,std::string message,bool digest_authenticate)
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
    std::string stringaRisposta;
        
    curl_easy_reset(curl); //reset all options
    if (digest_authenticate) 
    {
        curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC|CURLAUTH_DIGEST);
    }
    curl_easy_setopt(curl,CURLOPT_URL, APIendpoint.c_str() );
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,CurlWrapper::CurlWriteMemoryCallback);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&stringaRisposta); //the extra data to pass to WRITEFUNCTION (it's our allocated memory where we're writing the message bit by bit)
    curl_easy_setopt(curl,CURLOPT_TIMEOUT, 60);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 );
    
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
    return stringaRisposta;
}
    
//stays authenticated until curl_easy_cleanup
void CurlWrapper::digestAuthenticate(std::string username, std::string password,std::string APIendpoint)
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
}
    
/**
    * This static method is called by curl every time it gets a new chunk of data, and it appends it to the userdata
    * @param ptr contains the new data to write
    * @param size sizeof
    * @param nmemb how many (multiply by size to get real size)
    * @param stringaRisposta is the string to which the data will be appended;
    * @return 
    */
size_t CurlWrapper::CurlWriteMemoryCallback( char *ptr, size_t size, size_t nmemb, std::string *stringaRisposta)
{
    size_t realsize = size * nmemb;
    stringaRisposta->append(ptr, size*nmemb);
    return size * nmemb;
}