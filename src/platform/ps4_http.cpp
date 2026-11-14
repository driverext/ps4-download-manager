#include "dm/core.hpp"
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#include <sstream>
#include <stdexcept>
#include <vector>
extern "C" void dmSslTerm(int);extern "C" int dmHttpConfigure(int,unsigned int);
namespace dm {
static int check(int n){if(n<0)throw std::runtime_error("PS4 networking error "+std::to_string(n));return n;}
class SceTransport:public Transport{
 int pool=-1,ssl=-1,http=-1;
 void cleanup(){if(http>=0)sceHttpTerm(http);if(ssl>=0)dmSslTerm(ssl);if(pool>=0)sceNetPoolDestroy(pool);}
public:SceTransport(){static std::once_flag init;std::call_once(init,[]{check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET));check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL));check(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP));check(sceNetInit());});try{pool=check(sceNetPoolCreate("PS4DM",512*1024,0));ssl=check(sceSslInit(512*1024));http=check(sceHttpInit(pool,ssl,1024*1024));}catch(...){cleanup();throw;}}
 ~SceTransport(){cleanup();}
 void perform(const Request& r,Head head,Body body,std::atomic<bool>& stop)override{
 struct Handles{int t=-1,c=-1,r=-1;~Handles(){if(r>=0)sceHttpDeleteRequest(r);if(c>=0)sceHttpDeleteConnection(c);if(t>=0)sceHttpDeleteTemplate(t);}}h;
 h.t=check(sceHttpCreateTemplate(http,"PS4DownloadManager/0.1.1",ORBIS_HTTP_VERSION_1_1,1));
 // Verified flag values: server, hostname, dates, known CA, SNI. No bypass callback.
 check(sceHttpsEnableOption(h.t,0x01|0x04|0x08|0x10|0x20|0x80));check(dmHttpConfigure(h.t,r.timeout*1000000U));check(sceHttpSetConnectTimeOut(h.t,r.timeout*1000000U));check(sceHttpSetResolveTimeOut(h.t,r.timeout*1000000U));check(sceHttpSetSendTimeOut(h.t,r.timeout*1000000U));
 h.c=check(sceHttpCreateConnectionWithURL(h.t,r.url.c_str(),false));h.r=check(sceHttpCreateRequestWithURL2(h.c,r.method.c_str(),r.url.c_str(),r.body.size()));for(auto& x:r.headers)check(sceHttpAddRequestHeader(h.r,x.first.c_str(),x.second.c_str(),0));if(stop)throw std::runtime_error("Stopped");check(sceHttpSendRequest(h.r,r.body.empty()?nullptr:r.body.data(),r.body.size()));
 Response response;check(sceHttpGetStatusCode(h.r,&response.status));char* raw=nullptr;size_t size=0;check(sceHttpGetAllResponseHeaders(h.r,&raw,&size));if(!raw||size>128*1024)throw std::runtime_error("Invalid response headers");std::string headers(raw,size);while(!headers.empty()&&headers.back()==0)headers.pop_back();std::istringstream lines(headers);std::string line;while(std::getline(lines,line)){auto p=line.find(':');if(p!=std::string::npos)response.headers[lower(line.substr(0,p))]=trim(line.substr(p+1));}head(response);if(r.method=="HEAD")return;// PS4 worker threads can default to a 64 KiB stack. A 64 KiB local array
 // overflows that stack before the HTTPS receive path can run. Keep the
 // receive buffer on the heap and use a smaller bounded 16 KiB chunk.
 std::vector<char> buffer(16*1024);
 for(;;){if(stop){sceHttpAbortRequest(h.r);throw std::runtime_error("Stopped");}int n=check(sceHttpReadData(h.r,buffer.data(),buffer.size()));if(!n)break;body(buffer.data(),static_cast<size_t>(n));}
 }};std::unique_ptr<Transport> transport(){return std::unique_ptr<Transport>(new SceTransport);}
}
