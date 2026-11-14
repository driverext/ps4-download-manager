#include "dm/core.hpp"
#include "../platform/graphics.h"
#include "font.hpp"
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#ifdef DM_FS_READONLY
#include <cstring>
#include <sys/stat.h>
#endif
std::stringstream debugLogStream;
#ifdef DM_FS_READONLY
// Explicit opt-in read-only kernel stat probe. No writes, mkdir, or enumeration.
static std::string probeRootReadOnly() {
  alignas(16) unsigned char bytes[256]{};
  int rc = sceKernelStat("/data", reinterpret_cast<OrbisKernelStat*>(bytes));
  if (rc != 0) {
    char message[64];
    std::snprintf(message, sizeof(message), "FS PROBE FAIL: sceKernelStat rc=0x%08x", static_cast<unsigned>(rc));
    return message;
  }
  uint16_t mode = 0;
  std::memcpy(&mode, bytes + 8, sizeof(mode));
  return S_ISDIR(mode) ? "FS PROBE OK: /data is a directory" : "FS PROBE FAIL: /data is not directory";
}
#endif

#ifdef DM_UI_ONLY
// Absolutely no disk access in this integration stage.
static void bootLog(const char*) {}
#else
static void bootLog(const char* stage) { FILE* fp = fopen("/data/ps4dm_boot.log", "a"); if (fp) { fprintf(fp, "%s\n",stage); fflush(fp); fclose(fp); } }
#endif
namespace{
Color bg{11,18,32},fg{225,235,250},accent{64,210,208},panel{23,40,62};
void text(Scene2D& s,int x,int y,std::string value,Color color=fg){for(unsigned char c:value){if(x>1810)break;if(c<32||c>126)c='?';for(int row=0;row<30;row++)for(int col=0;col<16;col++)if(glyphs[c-32][row]&(1<<col))s.DrawPixel(x+col,y+row,color);x+=16;}}
std::string amount(uint64_t n){std::ostringstream s;s.precision(1);s<<std::fixed<<double(n)/(1024*1024)<<" MiB";return s.str();}
bool allowed(const std::string& p){return p.find("..") == std::string::npos&&(p.rfind("/data/PS4DownloadManager/",0)==0||p.rfind("/mnt/usb0/",0)==0||p.rfind("/mnt/usb1/",0)==0);}
}
int main(){
 bootLog("START: main entered");
 Scene2D screen(1920,1080,4);
 bootLog("START: graphics object created");
 if(!screen.Init(0xC000000,2)){bootLog("FAIL: graphics Init");return 1;}
 bootLog("OK: graphics initialized");
 OrbisUserServiceInitializeParams params{};
 params.priority=ORBIS_KERNEL_PRIO_FIFO_LOWEST;
 int userRc=sceUserServiceInitialize(&params);
 if(userRc<0){bootLog("FAIL: user service initialization");return 3;}
 bootLog("OK: user service initialized");
 int user=-1;
 int userGetRc=sceUserServiceGetInitialUser(&user);
 if(userGetRc<0){bootLog("FAIL: get initial user");return 4;}
 bootLog("OK: initial user acquired");
 int padInitRc=scePadInit();
 if(padInitRc<0){bootLog("FAIL: pad init");return 5;}
 bootLog("OK: pad initialized");
 int pad=scePadOpen(user,0,0,nullptr);
 if(pad<0){bootLog("FAIL: pad open");return 2;}
 bootLog("OK: pad opened");
 const std::string root="/data/PS4DownloadManager";dm::Settings cfg;cfg.directory=root+"/downloads";std::unique_ptr<dm::Engine> engine;std::unique_ptr<dm::Transport> net;std::unique_ptr<dm::Debrid> rd;std::string message;
#ifdef DM_UI_ONLY
 // Offline integration stage: deliberately do not create paths, instantiate the
 // download engine, or initialize HTTP / Real-Debrid.
 message="OFFLINE UI TEST: downloads and filesystem are disabled";
#else
 bootLog("START: persistent storage and network setup");
 try{dm::mkdirs(root);bootLog("OK: storage directory");auto c=dm::readJson(root+"/settings.json",dm::Json::object());cfg.concurrency=std::clamp(c.value("concurrency",2),1,4);cfg.retries=std::clamp(c.value("retries",3),0,6);cfg.timeout=std::clamp(c.value("timeout",30),5,120);cfg.autoResume=c.value("auto_resume",false);cfg.speedLimit=c.value("speed_limit",uint64_t(0));cfg.directory=c.value("directory",cfg.directory);if(!allowed(cfg.directory))throw std::runtime_error("Directory outside allowed roots");engine.reset(new dm::Engine(root,cfg));bootLog("OK: download engine");
 net=dm::transport();bootLog("OK: network transport");
 rd.reset(new dm::Debrid(*net,root));bootLog("OK: Real-Debrid state");
 }catch(const std::exception& e){message=e.what();bootLog("FAIL: caught startup exception");}
 bootLog("OK: startup initialization finished");
#endif
 std::vector<std::string> pages={"Home","Add download","Queue","Active downloads","Completed downloads","Real-Debrid account","Torrents","Files","Settings","Error details"};int page=0,selected=0,frame=0,key=0;uint32_t previous=0;bool keyboard=false,useRD=false;std::string input,purpose,group="Default",file,torrentId;const std::string keys="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/?&=.%_-+~#[]@!,$;()";
 dm::Json device,account=dm::Json::object(),history=dm::Json::array(),torrents=dm::Json::array(),torrent=dm::Json::object();double pollAt=0,torrentAt=0,spaceAt=0;uint64_t available=0;
 auto edit=[&](std::string what,std::string initial=""){purpose=what;input=initial;keyboard=true;key=0;};
 auto saveSettings=[&]{
#ifdef DM_UI_ONLY
 message="Settings changed in memory only. Offline UI test.";
#else
 dm::atomicJson(root+"/settings.json",{{"directory",cfg.directory},{"concurrency",cfg.concurrency},{"retries",cfg.retries},{"timeout",cfg.timeout},{"speed_limit",cfg.speedLimit},{"auto_resume",cfg.autoResume}});
 message="Saved. Restart app to apply engine settings.";
#endif
};
 bootLog("START: UI loop");
 for(;;){OrbisPadData data{};if(scePadReadState(pad,&data)<0){data.buttons=0;data.leftStick.x=128;data.leftStick.y=128;}uint32_t buttons=data.buttons;if(data.leftStick.x<55)buttons|=ORBIS_PAD_BUTTON_LEFT;if(data.leftStick.x>200)buttons|=ORBIS_PAD_BUTTON_RIGHT;if(data.leftStick.y<55)buttons|=ORBIS_PAD_BUTTON_UP;if(data.leftStick.y>200)buttons|=ORBIS_PAD_BUTTON_DOWN;auto pressed=buttons&~previous;previous=buttons;
 try{if(engine)engine->tick();
#ifndef DM_UI_ONLY
 if(dm::now()>spaceAt){spaceAt=dm::now()+5;available=dm::freeSpace(cfg.directory);}
#endif
if(rd&&!device.is_null()&&dm::now()>pollAt){pollAt=dm::now()+std::max(5,device.value("interval",5));if(dm::now()>device.at("deadline").get<double>()){device=nullptr;message="Authorization expired";}else if(rd->poll(device)){device=nullptr;account=rd->account();message="Real-Debrid authorized";}}if(rd&&page==6&&!torrentId.empty()&&dm::now()>torrentAt){torrentAt=dm::now()+15;torrent=rd->torrent(torrentId);}}catch(const std::exception& e){message=e.what();}
 std::vector<std::string> rows,fileList;std::vector<dm::Job> shown;
 if(page==0){rows=pages;rows.erase(rows.begin());}else if(page==1)rows={"Enter URL or magnet","Resolver: "+std::string(useRD?"Real-Debrid":"Direct HTTP(S)"),"Package: "+group};
 else if(page>=2&&page<=4&&engine){for(auto& j:engine->snapshot())if(page==2||(page==3&&(j.state=="active"||j.state=="queued"))||(page==4&&j.state=="completed")){shown.push_back(j);rows.push_back(j.name+" | "+j.state+" | "+amount(j.done)+" / "+amount(j.total));}}
 else if(page==5){rows={rd&&rd->loggedIn()?"Refresh account":"Authorize device","Logout and remove credentials","Load download history","Load supported hosts"};if(account.contains("username"))rows.push_back(account.value("username","")+" | "+account.value("type","")+" | premium seconds "+std::to_string(account.value("premium",0)));for(auto& h:history)rows.push_back(h.value("filename","History"));}
 else if(page==6){rows={"Refresh torrents","Add magnet","Enter torrent ID","Select file IDs (or all)","Download completed torrent links"};if(!torrentId.empty())rows.push_back("Current: "+torrentId+" "+torrent.value("status",""));if(torrent.contains("files"))for(auto& f:torrent["files"])rows.push_back(std::to_string(f.value("id",0))+" "+f.value("path",""));for(auto& t:torrents)rows.push_back(t.value("id","")+" "+t.value("status","")+" "+t.value("filename",""));}
 else if(page==7){
#ifdef DM_UI_ONLY
 rows={"File listing disabled in offline UI test"};
#else
 try{fileList=dm::files(cfg.directory);rows=fileList;}catch(const std::exception& e){message=e.what();}
#endif
}
 else if(page==8){
  rows={"Concurrency: "+std::to_string(cfg.concurrency),"Retries: "+std::to_string(cfg.retries),"Timeout: "+std::to_string(cfg.timeout)+" sec","Auto resume: "+std::string(cfg.autoResume?"yes":"no"),"Speed limit: "+std::to_string(cfg.speedLimit)+" B/s","Directory: "+cfg.directory,"Clear completed history","Save settings"};
#ifdef DM_FS_READONLY
  rows.push_back("Probe /data (READ-ONLY; press X)");
#endif
}
 else if(page==9)for(size_t at=0;at<message.size();at+=100)rows.push_back(message.substr(at,100));selected=std::clamp(selected,0,std::max(0,int(rows.size())-1));
 try{if(keyboard){int count=keys.size()+4;if(pressed&ORBIS_PAD_BUTTON_LEFT)key=(key+count-1)%count;if(pressed&ORBIS_PAD_BUTTON_RIGHT)key=(key+1)%count;if(pressed&ORBIS_PAD_BUTTON_UP)key=(key+count-12)%count;if(pressed&ORBIS_PAD_BUTTON_DOWN)key=(key+12)%count;if(pressed&ORBIS_PAD_BUTTON_SQUARE&&!input.empty())input.pop_back();if(pressed&ORBIS_PAD_BUTTON_CIRCLE)keyboard=false;
 if(pressed&ORBIS_PAD_BUTTON_CROSS){if(key<(int)keys.size()){if(input.size()<8192)input+=keys[key];}else if(key==(int)keys.size()){if(!input.empty())input.pop_back();}else if(key==(int)keys.size()+1)keyboard=false;else if(key==(int)keys.size()+2){if(input.size()<8192)input+='\n';}else{keyboard=false;
 if(purpose=="URL"){if(!engine||!net||(useRD&&!rd))throw std::runtime_error("Initialization failed");if(input.rfind("magnet:?",0)==0){if(!rd)throw std::runtime_error("Real-Debrid unavailable");torrentId=rd->magnet(input).at("id");page=6;}else{std::istringstream batch(input);std::string link;while(std::getline(batch,link)){link=dm::trim(link);if(link.empty())continue;dm::Resolved r;if(useRD){dm::DebridResolver resolver(*rd);r=resolver.resolve(link);}else{dm::DirectResolver resolver(*net);r=resolver.resolve(link);}engine->add(r.url,r.name,group);}page=2;}selected=0;}
 else if(purpose=="Package")group=input;else if(purpose=="Magnet"){torrentId=rd->magnet(input).at("id");torrent=rd->torrent(torrentId);}else if(purpose=="Torrent ID"){torrentId=input;torrent=rd->torrent(torrentId);}else if(purpose=="File IDs"){rd->select(torrentId,input);torrent=rd->torrent(torrentId);}else if(purpose=="Directory"){if(!allowed(input))throw std::runtime_error("Use app data or a USB subdirectory");cfg.directory=input;saveSettings();}else if(purpose=="Rename"){auto dest=cfg.directory+"/"+dm::safeName(input);if(dm::exists(dest)||rename((cfg.directory+"/"+file).c_str(),dest.c_str()))throw std::runtime_error("Rename failed or destination exists");}else if(purpose=="Type DELETE to confirm"){if(input=="DELETE"&&unlink((cfg.directory+"/"+file).c_str()))throw std::runtime_error("Delete failed");}
 }}}
 else{if(pressed&ORBIS_PAD_BUTTON_OPTIONS){page=8;selected=0;}if(pressed&ORBIS_PAD_BUTTON_CIRCLE){page=0;selected=0;}if(pressed&ORBIS_PAD_BUTTON_UP)selected=std::max(0,selected-1);if(pressed&ORBIS_PAD_BUTTON_DOWN)selected=std::min(std::max(0,int(rows.size())-1),selected+1);
 if((pressed&ORBIS_PAD_BUTTON_SQUARE)&&!shown.empty()){auto j=shown[selected];if(j.state=="paused"||j.state=="error")engine->resume(j.id);else engine->pause(j.id);}if((pressed&ORBIS_PAD_BUTTON_TRIANGLE)&&!shown.empty())engine->cancel(shown[selected].id);
 if((pressed&ORBIS_PAD_BUTTON_TRIANGLE)&&page==7&&!fileList.empty()){file=fileList[selected];if(file.find(".part")!=std::string::npos)throw std::runtime_error("Partial files are managed by queue");edit("Type DELETE to confirm");}
 if(pressed&ORBIS_PAD_BUTTON_CROSS){if(page==0){page=selected+1;selected=0;}else if(page==1){if(selected==0)edit("URL","https://");if(selected==1)useRD=!useRD;if(selected==2)edit("Package",group);}else if(page>=2&&page<=4&&!shown.empty()){message=shown[selected].error.empty()?"Package: "+shown[selected].package:shown[selected].error;page=9;selected=0;}
 else if(page==5&&rd){if(selected==0){if(rd->loggedIn())account=rd->account();else{device=rd->begin();pollAt=device.at("next_poll");}}if(selected==1){rd->logout();account=dm::Json::object();device=nullptr;}if(selected==2)history=rd->history();if(selected==3){message=rd->hosts().dump();page=9;selected=0;}}
 else if(page==6&&rd){if(selected==0){torrents=rd->torrents();if(!torrentId.empty())torrent=rd->torrent(torrentId);}if(selected==1)edit("Magnet","magnet:?");if(selected==2)edit("Torrent ID",torrentId);if(selected==3)edit("File IDs","all");if(selected==4&&engine){torrent=rd->torrent(torrentId);if(torrent.value("status","")!="downloaded")throw std::runtime_error("Torrent not ready");for(auto& link:torrent.at("links")){auto r=rd->unrestrict(link);engine->add(r.url,r.name,torrent.value("filename","Torrent"));}}}
 else if(page==7&&!fileList.empty()){file=fileList[selected];if(file.find(".part")!=std::string::npos)throw std::runtime_error("Partial files are managed by queue");edit("Rename",file);}
 else if(page==8){if(selected==0)cfg.concurrency=cfg.concurrency%4+1;if(selected==1)cfg.retries=(cfg.retries+1)%7;if(selected==2)cfg.timeout=cfg.timeout>=120?15:cfg.timeout+15;if(selected==3)cfg.autoResume=!cfg.autoResume;if(selected==4)cfg.speedLimit=cfg.speedLimit?0:1024*1024;if(selected==5)edit("Directory",cfg.directory);if(selected==6&&engine)engine->clearCompleted();if(selected==7)saveSettings();
#ifdef DM_FS_READONLY
 if(selected==8){ message="FS PROBE STARTING"; message=probeRootReadOnly(); }
#endif
}
 }}}catch(const std::exception& e){message=e.what();}
 if(frame==0)bootLog("START: first graphics frame");
 screen.FrameBufferFill(bg);screen.DrawRectangle(0,0,1920,125,panel);text(screen,80,45,"PS4 DOWNLOAD MANAGER",accent);text(screen,1050,45,
#ifdef DM_UI_ONLY
 "STAGE FS READ-ONLY | NO NET"
#else
 "0.1.1 experimental | GoldHEN"
#endif
 );text(screen,80,155,pages[page],accent);text(screen,1150,155,"Free: "+amount(available));
 if(keyboard){text(screen,80,215,purpose);text(screen,80,265,input.size()>100?input.substr(input.size()-100):input);for(int i=0;i<(int)keys.size()+4;i++){int x=80+(i%12)*140,y=350+(i/12)*65;if(i==key)screen.DrawRectangle(x-8,y-5,132,48,panel);text(screen,x,y,i<(int)keys.size()?std::string(1,keys[i]):i==(int)keys.size()?"BACKSPACE":i==(int)keys.size()+1?"CANCEL":i==(int)keys.size()+2?"NEWLINE":"DONE",i==key?accent:fg);}}
 else{int start=selected/12*12;for(int i=start;i<(int)rows.size()&&i<start+12;i++){int y=230+(i-start)*53;if(i==selected)screen.DrawRectangle(65,y-5,1790,48,panel);text(screen,85,y,rows[i],i==selected?accent:fg);}if(!device.is_null())text(screen,80,880,"Authorize: "+device.value("verification_url","")+" code "+device.value("user_code",""),accent);if(!shown.empty()){auto j=shown[std::min(selected,int(shown.size())-1)];if(j.total)screen.DrawRectangle(80,910,int(1720*std::min(1.0,double(j.done)/j.total)),10,accent);text(screen,80,930,amount(uint64_t(j.speed))+"/s | ETA "+(j.speed>0&&j.total>j.done?std::to_string(int((j.total-j.done)/j.speed))+"s":"unknown"));}}
 text(screen,80,995,keyboard?"D-pad: key   X: enter   Square: backspace   Circle: cancel":"X: select   Circle: home   Square: pause/resume   Triangle: cancel/delete   Options: settings");if(!message.empty()&&page!=9&&!keyboard)text(screen,80,960,message.substr(0,105));screen.SubmitFlip(frame);if(frame==0)bootLog("OK: first flip submitted");
 screen.FrameWait(frame++);if(frame==1)bootLog("OK: first flip completed");screen.FrameBufferSwap();
 }}
