#include "dm/core.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>
namespace dm {
Engine::Engine(std::string r,Settings c):root(r),cfg(c){if(c.concurrency<1||c.concurrency>4||c.retries<0||c.retries>6||c.timeout<1||c.timeout>120)throw std::runtime_error("Invalid engine settings");mkdirs(root);mkdirs(cfg.directory);jobs=readJson(root+"/queue.json",Json::array()).get<std::vector<Job>>();for(auto& j:jobs){Url::parse(j.url);number(j.id);if(j.name!=safeName(j.name))throw std::runtime_error("Unsafe queue filename");if(j.state=="active"||j.state=="queued")j.state=cfg.autoResume?"queued":"paused";}save();}
Engine::~Engine(){for(auto& w:workers)w.second->stop=true;for(auto& w:workers)if(w.second->thread.joinable())w.second->thread.join();}
void Engine::save(){atomicJson(root+"/queue.json",jobs);}
void Engine::update(const Job& x){std::lock_guard<std::mutex> lock(mutex);for(auto& j:jobs)if(j.id==x.id){auto state=j.state;j=x;if((state=="paused"||state=="cancelled")&&x.state!="completed")j.state=state;return;}}
std::vector<Job> Engine::snapshot()const{std::lock_guard<std::mutex> lock(mutex);return jobs;}
std::string Engine::add(std::string u,std::string name,std::string group,bool paused){u=Url::parse(trim(u)).str();std::lock_guard<std::mutex> lock(mutex);for(auto& j:jobs)if(j.url==u&&j.state!="cancelled")throw std::runtime_error("Duplicate download");uint64_t id=std::chrono::system_clock::now().time_since_epoch()/std::chrono::milliseconds(1);for(auto& j:jobs)id=std::max(id,number(j.id)+1);Job j;j.id=std::to_string(id);j.url=u;auto path=Url::parse(u).path;path=path.substr(0,path.find('?'));j.name=safeName(name.empty()?path.substr(path.find_last_of('/')+1):name);j.package=group;if(paused)j.state="paused";jobs.push_back(j);save();return j.id;}
void Engine::tick(){std::lock_guard<std::mutex> lock(mutex);for(auto i=workers.begin();i!=workers.end();)if(i->second->finished){i->second->thread.join();i=workers.erase(i);save();}else ++i;for(auto& j:jobs)if(j.state=="queued"&&!workers.count(j.id)&&workers.size()<(size_t)cfg.concurrency){auto w=std::unique_ptr<Worker>(new Worker);auto ptr=w.get();workers[j.id]=std::move(w);j.state="active";try{ptr->thread=std::thread([this,id=j.id,ptr]{try{run(id,ptr);}catch(const std::exception& e){std::lock_guard<std::mutex> l(mutex);for(auto& j:jobs)if(j.id==id){j.state="error";j.error=e.what();}}ptr->finished=true;});}catch(...){workers.erase(j.id);j.state="error";j.error="Cannot create transfer thread";save();}}}
void Engine::pause(const std::string& id){std::lock_guard<std::mutex> l(mutex);if(workers.count(id))workers.at(id)->stop=true;for(auto& j:jobs)if(j.id==id&&j.state!="completed")j.state="paused";save();}
void Engine::resume(const std::string& id){std::lock_guard<std::mutex> l(mutex);if(workers.count(id))return;for(auto& j:jobs)if(j.id==id&&(j.state=="paused"||j.state=="error"))j.state="queued";save();}
void Engine::cancel(const std::string& id){pause(id);std::lock_guard<std::mutex> l(mutex);for(auto& j:jobs)if(j.id==id&&j.state!="completed")j.state="cancelled";save();}
void Engine::clearCompleted(){std::lock_guard<std::mutex> l(mutex);jobs.erase(std::remove_if(jobs.begin(),jobs.end(),[](const Job& j){return j.state=="completed";}),jobs.end());save();}
void Engine::run(std::string id,Worker* worker){Job j;for(auto& x:snapshot())if(x.id==id)j=x;auto path=cfg.directory+"/"+j.id+"-"+j.name,part=path+".part",meta=part+".json";auto net=transport();
 for(int attempt=0;attempt<=cfg.retries&&!worker->stop;attempt++){int fd=-1;try{
 if(exists(path)){auto ready=readJson(meta,Json::object());if(ready.value("complete",false)&&ready.value("url","")==j.url&&ready.value("size",uint64_t(0))==fileSize(path)){j.state="completed";j.done=j.total=fileSize(path);update(j);return;}throw std::runtime_error("Destination already exists; inspect file");}
 auto m=readJson(meta,Json::object());uint64_t offset=fileSize(part);std::string etag=m.value("etag","");if(m.value("url","")!=j.url||etag.empty()||etag.rfind("W/",0)==0)offset=0;
 Request r;r.url=j.url;r.timeout=cfg.timeout;r.headers["Accept-Encoding"]="identity";if(offset){r.headers["Range"]="bytes="+std::to_string(offset)+"-";r.headers["If-Range"]=etag;}
 j.done=offset;j.total=0;j.speed=0;j.error="";double start=now(),last=start;uint64_t initial=offset,expected=0,received=0;bool known=false;
 fetch(*net,r,[&](const Response& h){
 if(h.status!=200&&h.status!=206)throw std::runtime_error("Download HTTP "+std::to_string(h.status));
 if(h.headers.count("content-type")&&lower(h.headers.at("content-type")).find("text/html")!=std::string::npos)throw std::runtime_error("Server returned HTML");
 if(h.headers.count("content-encoding")&&lower(h.headers.at("content-encoding"))!="identity")throw std::runtime_error("Unexpected content encoding");
 if(h.status==200){offset=0;initial=0;j.done=0;}else{
 if(!h.headers.count("content-range"))throw std::runtime_error("Missing Content-Range");auto range=h.headers.at("content-range");auto dash=range.find('-'),slash=range.find('/');if(range.rfind("bytes ",0)!=0||dash==std::string::npos||slash==std::string::npos||dash>=slash)throw std::runtime_error("Invalid Content-Range");auto a=number(range.substr(6,dash-6)),b=number(range.substr(dash+1,slash-dash-1)),total=number(range.substr(slash+1));if(a!=offset||b<a||total<=b)throw std::runtime_error("Invalid range bounds");expected=b-a+1;known=true;j.total=total;if(offset&&h.headers.count("etag")&&h.headers.at("etag")!=etag)throw std::runtime_error("Entity changed during resume");}
 if(h.headers.count("content-length")){auto n=number(h.headers.at("content-length"));if(known&&n!=expected)throw std::runtime_error("Range length mismatch");known=true;expected=n;if(h.status==200)j.total=n;}
 #ifdef DM_BETA
 // Bound early hardware tests to small files until storage capacity and
 // write/flush compatibility have been verified on the target console.
 if (known && (expected > 64ULL*1024*1024 || offset > 64ULL*1024*1024))
  throw std::runtime_error("Beta max download: 64 MiB");
#else
 if(known&&freeSpace(cfg.directory)<expected)throw std::runtime_error("Insufficient free space");
#endif
 j.etag=h.headers.count("etag")?h.headers.at("etag"):"";
 atomicJson(meta,{{"url",j.url},{"etag",j.etag}});fd=open(part.c_str(),O_WRONLY|O_CREAT|O_NOFOLLOW|(offset?0:O_TRUNC),0600);if(fd<0)throw std::runtime_error("Cannot open partial file");if(lseek(fd,(off_t)offset,SEEK_SET)<0)throw std::runtime_error("Partial-file seek failed");
 },[&](const char* p,size_t n){if(worker->stop)throw std::runtime_error("Stopped");if(known&&(received>expected||n>expected-received))throw std::runtime_error("Body exceeds declared length");
#ifdef DM_BETA
 if(j.done > 64ULL*1024*1024 || n > 64ULL*1024*1024-j.done)
  throw std::runtime_error("Beta max download: 64 MiB");
#endif
 size_t wrote=0;while(wrote<n){auto rc=write(fd,p+wrote,n-wrote);if(rc<=0)throw std::runtime_error("Disk write failed or disk full");wrote+=rc;}received+=n;j.done+=n;j.speed=(j.done-initial)/std::max(0.001,now()-start);if(now()-last>0.2){update(j);last=now();}if(cfg.speedLimit)while(!worker->stop&&now()-start<double(j.done-initial)/cfg.speedLimit)sleepMs(20);},worker->stop);
 if(known&&received!=expected)throw std::runtime_error("Truncated response");if(worker->stop)throw std::runtime_error("Stopped");if(fd<0)throw std::runtime_error("No response headers");if(fsync(fd))throw std::runtime_error("Disk flush failed");close(fd);fd=-1;
 atomicJson(meta,{{"url",j.url},{"etag",j.etag},{"complete",true},{"size",j.done}});if(exists(path)||rename(part.c_str(),path.c_str()))throw std::runtime_error("Completion rename failed");j.state="completed";j.total=j.done;j.speed=0;update(j);{std::lock_guard<std::mutex> l(mutex);save();}unlink(meta.c_str());return;
 }catch(const std::exception& e){if(fd>=0){fsync(fd);close(fd);}j.error=e.what();if(worker->stop)break;if(attempt==cfg.retries){j.state="error";j.speed=0;update(j);return;}for(int i=0;i<(1<<std::min(attempt,6))*10&&!worker->stop;i++)sleepMs(100);}}
 std::lock_guard<std::mutex> l(mutex);for(auto& x:jobs)if(x.id==id){j.state=x.state=="cancelled"?"cancelled":"paused";j.speed=0;x=j;}
}
}
