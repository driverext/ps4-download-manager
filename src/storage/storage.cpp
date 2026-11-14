#include "dm/core.hpp"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <fstream>
#include <chrono>
#include <stdexcept>
#include <cstdio>
#include <cerrno>
namespace dm {
static_assert(sizeof(off_t)>=8,"64-bit file offsets required");
double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}void sleepMs(int n){std::this_thread::sleep_for(std::chrono::milliseconds(n));}
bool exists(const std::string& p){struct stat s;return lstat(p.c_str(),&s)==0;}
uint64_t fileSize(const std::string& p){struct stat s;if(lstat(p.c_str(),&s)){if(errno==ENOENT)return 0;throw std::runtime_error("Cannot stat file");}if(!S_ISREG(s.st_mode))throw std::runtime_error("Not a regular file");return s.st_size;}
static void fsTrace(const char* msg, const char* path = nullptr, int result = 0, int error = 0) {
 FILE* f = fopen("/data/ps4dm_boot.log", "a");
 if (!f) return;
 fprintf(f, "FS: %s path=%s result=%d errno=%d\n", msg, path ? path : "-", result, error);
 fflush(f);
 fclose(f);
}
void mkdirs(const std::string& p) {
 fsTrace("mkdirs begin", p.c_str());
 if (p.empty()) throw std::runtime_error("Empty path");
 for (size_t i=1;i<=p.size();i++) {
  if (i!=p.size() && p[i]!='/') continue;
  auto part=p.substr(0,i);
  fsTrace("before lstat",part.c_str());
  struct stat info{};
  errno=0;
  int rc=lstat(part.c_str(), &info);
  int savedErrno=errno;
  fsTrace("after lstat",part.c_str(),rc,savedErrno);
  if(rc==0) {
   if(!S_ISDIR(info.st_mode)||S_ISLNK(info.st_mode)) throw std::runtime_error("Unsafe directory");
  } else {
   if(savedErrno!=ENOENT) throw std::runtime_error("Cannot inspect directory");
   fsTrace("before mkdir",part.c_str());
   errno=0;
   int makeRc=mkdir(part.c_str(),0700);
   int makeErrno=errno;
   fsTrace("after mkdir",part.c_str(),makeRc,makeErrno);
   if(makeRc!=0) throw std::runtime_error("Cannot create directory");
  }
 }
 fsTrace("mkdirs finished",p.c_str());
}

uint64_t freeSpace(const std::string& p){struct statvfs s;if(statvfs(p.c_str(),&s))throw std::runtime_error("Cannot query free space");return uint64_t(s.f_bavail)*s.f_frsize;}
void atomicJson(const std::string& path,const Json& j){auto text=j.dump(2),temp=path+".tmp";int fd=open(temp.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);if(fd<0)throw std::runtime_error("Cannot save metadata");size_t n=0;while(n<text.size()){auto rc=write(fd,text.data()+n,text.size()-n);if(rc<=0){close(fd);throw std::runtime_error("Metadata write failed");}n+=rc;}if(fsync(fd)){close(fd);throw std::runtime_error("Metadata flush failed");}close(fd);if(rename(temp.c_str(),path.c_str()))throw std::runtime_error("Metadata rename failed");}
Json readJson(const std::string& p,Json fallback){if(!exists(p))return fallback;std::ifstream f(p);return Json::parse(f);}
std::vector<std::string> files(const std::string& p){std::vector<std::string> result;auto dir=opendir(p.c_str());if(!dir)throw std::runtime_error("Cannot list files");while(auto e=readdir(dir)){std::string n=e->d_name;if(n!="."&&n!="..")result.push_back(n);}closedir(dir);return result;}
void to_json(Json& j,const Job& x){j={{"id",x.id},{"url",x.url},{"name",x.name},{"package",x.package},{"state",x.state},{"error",x.error},{"etag",x.etag},{"done",x.done},{"total",x.total}};}
void from_json(const Json& j,Job& x){x.id=j.at("id");x.url=j.at("url");x.name=j.at("name");x.package=j.value("package","Default");x.state=j.value("state","paused");x.error=j.value("error","");x.etag=j.value("etag","");x.done=j.value("done",uint64_t(0));x.total=j.value("total",uint64_t(0));}
}
