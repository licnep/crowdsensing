#include "CurlWrapper.h"


CurlWrapper::CurlWrapper(void)
{
	curl = curl_easy_init();
}


CurlWrapper::~CurlWrapper(void)
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
std::string CurlWrapper::sendMessage(int method,std::string APIendpoint,std::string message)
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
    curl_easy_setopt(curl,CURLOPT_TIMEOUT, 60);
    
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
        
    std::string result(memoryOutput.memory); //copy output in result string
    free(memoryOutput.memory);
    return result;
}
    
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
    //stays authenticated until curl_easy_cleanup
}
    
/**
    * This static method is called by curl every time it gets a new chunk of data, and it appends it to the userdata
    * @param ptr contains the new data to write
    * @param size sizeof
    * @param nmemb how many (multiply by size to get real size)
    * @param userdata must be of memoryStructForCurl type. Normally a CrowdSensing object passes its own memoryOutput;
    * @return 
    */
size_t CurlWrapper::CurlWriteMemoryCallback( char *ptr, size_t size, size_t nmemb, memoryStructForCurl *userdata)
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