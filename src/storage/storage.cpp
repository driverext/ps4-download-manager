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
#include <cstring>
#include <cstdint>
#ifdef PS4
#include <orbis/libkernel.h>
#endif
namespace dm {
static_assert(sizeof(off_t)>=8,"64-bit file offsets required");
double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}void sleepMs(int n){std::this_thread::sleep_for(std::chrono::milliseconds(n));}
static void fsTrace(const char* msg, const char* path = nullptr, int result = 0, int error = 0) {
 FILE* f = fopen("/data/ps4dm_boot.log", "a");
 if (!f) return;
 fprintf(f, "FS: %s path=%s result=%d errno=%d\n", msg, path ? path : "-", result, error);
 fflush(f);
 fclose(f);
}

#ifdef PS4
// The published OpenOrbis SDK headers may have an incompatible stat layout.
// Never pass OrbisKernelStat as a stack object to the kernel ABI.
// The PS4 kernel writes a 120-byte stat record (mode at 0x08, size at 0x48).
// Allocate excess space and copy fields out of the raw record individually.
// Layout provenance: orbis-ports/orbis-compat src/orbis_stat.cpp.
struct KernelStatRaw {
 alignas(16) unsigned char bytes[256]{};
};
static int kernelStat(const std::string& path, KernelStatRaw& raw) {
 return sceKernelStat(path.c_str(), reinterpret_cast<OrbisKernelStat*>(raw.bytes));
}
static uint16_t kernelMode(const KernelStatRaw& raw) {
 uint16_t value=0;
 std::memcpy(&value, raw.bytes + 0x08, sizeof(value));
 return value;
}
static int64_t kernelSize(const KernelStatRaw& raw) {
 int64_t value=0;
 std::memcpy(&value, raw.bytes + 0x48, sizeof(value));
 return value;
}
#endif

bool exists(const std::string& p) {
#ifdef PS4
 KernelStatRaw info;
 return kernelStat(p, info)==0;
#else
 struct stat info{};
 return lstat(p.c_str(), &info)==0;
#endif
}
uint64_t fileSize(const std::string& p) {
#ifdef PS4
 KernelStatRaw info;
 const int rc=kernelStat(p,info);
 if (rc!=0) {
  // sceKernelStat returns a kernel error code, not POSIX -1 / errno.
  const uint32_t code=static_cast<uint32_t>(rc);
  if ((code & 0xffff0000u)==0x80020000u && (code & 0xffffu)==ENOENT) return 0;
  throw std::runtime_error("Cannot stat file");
 }
 if (!S_ISREG(kernelMode(info))) throw std::runtime_error("Not a regular file");
 const int64_t n=kernelSize(info);
 if(n<0) throw std::runtime_error("Invalid file size");
 return static_cast<uint64_t>(n);
#else
 struct stat info{};
 if (lstat(p.c_str(), &info)) {
  if (errno==ENOENT) return 0;
  throw std::runtime_error("Cannot stat file");
 }
 if (!S_ISREG(info.st_mode)) throw std::runtime_error("Not a regular file");
 return static_cast<uint64_t>(info.st_size);
#endif
}
void mkdirs(const std::string& p) {
 fsTrace("mkdirs begin", p.c_str());
 if (p.empty() || p[0]!='/') throw std::runtime_error("Absolute directory path required");
 for (size_t i=1;i<=p.size();i++) {
  if(i!=p.size() && p[i]!='/') continue;
  const std::string part=p.substr(0,i);
  if(part.empty()) continue;
#ifdef PS4
  KernelStatRaw info;
  fsTrace("before sceKernelStat",part.c_str());
  int rc=kernelStat(part,info);
  fsTrace("after sceKernelStat",part.c_str(),rc);
  if(rc==0) {
   if(!S_ISDIR(kernelMode(info))) throw std::runtime_error("Not a directory");
  } else {
   const uint32_t code=static_cast<uint32_t>(rc);
   // Only create paths confirmed missing. Never call mkdir for an arbitrary
   // failure such as access denied, invalid pointer, or unsupported syscall.
   if ((code & 0xffff0000u)!=0x80020000u || (code & 0xffffu)!=ENOENT)
    throw std::runtime_error("Cannot inspect directory");
   fsTrace("before sceKernelMkdir",part.c_str());
   int mk=sceKernelMkdir(part.c_str(),0700);
   fsTrace("after sceKernelMkdir",part.c_str(),mk);
   KernelStatRaw check;
   const int again=kernelStat(part,check);
   fsTrace("after mkdir stat check",part.c_str(),again);
   if(again!=0 || !S_ISDIR(kernelMode(check))) throw std::runtime_error("Cannot create directory");
  }
#else
  struct stat info{};
  if(lstat(part.c_str(), &info)==0) {
   if(!S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) throw std::runtime_error("Unsafe directory");
  } else {
   if(errno!=ENOENT || mkdir(part.c_str(),0700)) throw std::runtime_error("Cannot create directory");
  }
#endif
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
