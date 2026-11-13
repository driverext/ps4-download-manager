#include "dm/core.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <stdexcept>
namespace dm {
class Curl:public Transport{
 struct State{Head head;Body body;Response h;std::exception_ptr error;std::atomic<bool>* stop;};
 static size_t header(char* p,size_t a,size_t b,void* ctx){auto& s=*(State*)ctx;try{std::string l(p,a*b);if(l.rfind("HTTP/",0)==0){s.h={};s.h.status=std::stoi(l.substr(l.find(' ')+1));}else if(l=="\r\n"){if(s.h.status>=200)s.head(s.h);}else{auto at=l.find(':');if(at!=std::string::npos)s.h.headers[lower(l.substr(0,at))]=trim(l.substr(at+1));}return a*b;}catch(...){s.error=std::current_exception();return 0;}}
 static size_t write(char* p,size_t a,size_t b,void* ctx){auto& s=*(State*)ctx;try{if(s.stop->load())return 0;s.body(p,a*b);return a*b;}catch(...){s.error=std::current_exception();return 0;}}
 static int progress(void* p,curl_off_t,curl_off_t,curl_off_t,curl_off_t){return ((State*)p)->stop->load()?1:0;}
public:void perform(const Request& r,Head h,Body b,std::atomic<bool>& stop)override{
 static const int init=curl_global_init(CURL_GLOBAL_DEFAULT);if(init)throw std::runtime_error("curl initialization failed");auto c=curl_easy_init();if(!c)throw std::runtime_error("curl allocation failed");State s{h,b,{},nullptr,&stop};curl_slist* headers=nullptr;for(auto& x:r.headers)headers=curl_slist_append(headers,(x.first+": "+x.second).c_str());
 curl_easy_setopt(c,CURLOPT_URL,r.url.c_str());curl_easy_setopt(c,CURLOPT_CUSTOMREQUEST,r.method.c_str());if(r.method=="HEAD")curl_easy_setopt(c,CURLOPT_NOBODY,1L);if(!r.body.empty()){curl_easy_setopt(c,CURLOPT_POSTFIELDS,r.body.data());curl_easy_setopt(c,CURLOPT_POSTFIELDSIZE_LARGE,(curl_off_t)r.body.size());}
 curl_easy_setopt(c,CURLOPT_HTTPHEADER,headers);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,(long)r.timeout);curl_easy_setopt(c,CURLOPT_LOW_SPEED_LIMIT,1L);curl_easy_setopt(c,CURLOPT_LOW_SPEED_TIME,(long)r.timeout);curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);
 curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,header);curl_easy_setopt(c,CURLOPT_HEADERDATA,&s);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,write);curl_easy_setopt(c,CURLOPT_WRITEDATA,&s);curl_easy_setopt(c,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(c,CURLOPT_XFERINFOFUNCTION,progress);curl_easy_setopt(c,CURLOPT_XFERINFODATA,&s);if(const char* ca=std::getenv("DM_CA_BUNDLE"))curl_easy_setopt(c,CURLOPT_CAINFO,ca);
 auto rc=curl_easy_perform(c);curl_slist_free_all(headers);curl_easy_cleanup(c);if(s.error)std::rethrow_exception(s.error);if(rc!=CURLE_OK)throw std::runtime_error(stop?"Stopped":curl_easy_strerror(rc));
 }};std::unique_ptr<Transport> transport(){return std::unique_ptr<Transport>(new Curl);}
}
