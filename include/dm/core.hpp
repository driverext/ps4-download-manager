#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
namespace dm {
using Json=nlohmann::json;using Headers=std::map<std::string,std::string>;
struct Url{std::string scheme,authority,path;static Url parse(const std::string&);std::string str()const;};
std::string lower(std::string),trim(std::string),encode(const std::string&),safeName(std::string),redirect(const std::string&,const std::string&);
uint64_t number(const std::string&);double now();void sleepMs(int);
struct Request{std::string url,method="GET",body;Headers headers;int timeout=30;};
struct Response{int status=0;Headers headers;};
using Head=std::function<void(const Response&)>;using Body=std::function<void(const char*,size_t)>;
class Transport{public:virtual ~Transport()=default;virtual void perform(const Request&,Head,Body,std::atomic<bool>&)=0;};
std::unique_ptr<Transport> transport();void fetch(Transport&,Request,Head,Body,std::atomic<bool>&);Json requestJson(Transport&,Request,std::atomic<bool>&);
bool exists(const std::string&);uint64_t fileSize(const std::string&),freeSpace(const std::string&);void mkdirs(const std::string&);void atomicJson(const std::string&,const Json&);Json readJson(const std::string&,Json);std::vector<std::string> files(const std::string&);
struct Settings{int concurrency=2,retries=3,timeout=30;uint64_t speedLimit=0;bool autoResume=false;std::string directory;};
struct Job{std::string id,url,name,package,state="queued",error,etag;uint64_t done=0,total=0;double speed=0;};
void to_json(Json&,const Job&);void from_json(const Json&,Job&);
class Engine{
 std::string root;Settings cfg;mutable std::mutex mutex;std::vector<Job> jobs;
 struct Worker{std::thread thread;std::atomic<bool> stop{false},finished{false};};std::map<std::string,std::unique_ptr<Worker>> workers;
 void save();void update(const Job&);void run(std::string,Worker*);
public:Engine(std::string,Settings);~Engine();std::string add(std::string,std::string="",std::string="Default");std::vector<Job> snapshot()const;void tick();void pause(const std::string&);void resume(const std::string&);void cancel(const std::string&);void clearCompleted();
};
struct Resolved{std::string url,name;uint64_t size=0;};
class ILinkResolver{public:virtual ~ILinkResolver()=default;virtual Resolved resolve(const std::string&)=0;};
class DirectResolver:public ILinkResolver{Transport& net;public:explicit DirectResolver(Transport& t):net(t){}Resolved resolve(const std::string&)override;};
class Debrid{
 Transport& net;std::string root;Json credentials;std::atomic<bool> stop{false};double next=0,pollAt=0;std::mutex gate;
 Json call(std::string,std::string="GET",std::string="",bool=true);void token(const std::string&);
public:Debrid(Transport&,std::string);bool loggedIn()const;Json begin();bool poll(const Json&);void logout();Json account(),hosts(),history(),torrents(),torrent(const std::string&),magnet(const std::string&);void select(const std::string&,const std::string&);Resolved unrestrict(const std::string&);
};
class DebridResolver:public ILinkResolver{Debrid& rd;public:explicit DebridResolver(Debrid& r):rd(r){}Resolved resolve(const std::string& u)override{return rd.unrestrict(u);}};
}
