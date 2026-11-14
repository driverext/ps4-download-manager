#include "dm/phone_share.hpp"
#include "dm/core.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <cstdlib>
#include <chrono>
#include <algorithm>

namespace dm {
namespace {
const char* html=R"PAGE(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Send to PS4</title>
<style>body{font:16px system-ui;background:#0b1220;color:#e1ebfa;margin:0;padding:24px}
main{max-width:540px;margin:12vh auto;background:#17283e;border-radius:18px;padding:28px}
h1{margin-top:0}label{display:block;margin-bottom:9px}
input,button{box-sizing:border-box;width:100%;font:inherit;padding:14px;border-radius:9px}
input{background:#f7fbff;color:#141d2b;border:0}button{border:0;background:#40d2d0;color:#091723;font-weight:700;margin-top:14px}
small{display:block;color:#aabdd2;margin-top:20px}</style></head><body><main>
<h1>Send a link to PS4</h1><form action="/submit" method="post" autocomplete="off">
<label for="url">Direct download URL</label><input id="url" name="url" type="url" maxlength="2048"
placeholder="https://example.com/file.zip" required autofocus>
<button type="submit">Send for approval</button></form>
<small>After sending, open Settings on your PS4 and select Accept phone link.
Only one pending link is allowed. The PS4 must approve it before download.</small>
</main></body></html>)PAGE";
const char* sent=R"PAGE(<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>Link sent</title></head>
<body style="font:18px system-ui;background:#0b1220;color:#e1ebfa;padding:30px">
<h2>Link sent to PS4</h2><p>On your PS4, open Settings and select Accept phone link.</p>
<p><a style="color:#40d2d0" href="/">Send another</a></p></body></html>)PAGE";
void respond(int client,int code,const std::string& content) {
  const char* status=code==200?"200 OK":code==400?"400 Bad Request":
    code==404?"404 Not Found":code==405?"405 Method Not Allowed":
    code==409?"409 Conflict":"413 Payload Too Large";
  std::string out="HTTP/1.1 "+std::string(status)+"\r\nContent-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"
    "Content-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; form-action 'self'\r\n"
    "Connection: close\r\nContent-Length: "+std::to_string(content.size())+"\r\n\r\n"+content;
  size_t off=0;
  while(off<out.size()) {
    const auto n=send(client,out.data()+off,out.size()-off,0);
    if(n<=0)break;
    off+=static_cast<size_t>(n);
  }
}
int fromHex(char c) {
  if(c>='0'&&c<='9')return c-'0';
  if(c>='a'&&c<='f')return c-'a'+10;
  if(c>='A'&&c<='F')return c-'A'+10;
  return -1;
}
std::string decode(const std::string& s) {
  std::string o;
  for(size_t i=0;i<s.size();i++) {
    if(s[i]=='+')o+=' ';
    else if(s[i]=='%'&&i+2<s.size()){
      const int a=fromHex(s[i+1]),b=fromHex(s[i+2]);
      if(a<0||b<0)throw std::runtime_error("Bad escape");
      o+=static_cast<char>((a<<4)|b);i+=2;
    }else if(s[i]=='%')throw std::runtime_error("Bad escape");
    else o+=s[i];
  }
  return o;
}
void handleClient(PhoneShareServer& server,int client) {
  // A phone on the same Wi-Fi may connect, but no request is auto-approved.
  timeval timeout{3,0};
  setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
  setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
  std::string req;char buffer[1024];
  size_t headEnd=std::string::npos;
  while(req.size()<8192) {
    const auto n=recv(client,buffer,sizeof(buffer),0);
    if(n<=0)break;
    req.append(buffer,static_cast<size_t>(n));
    headEnd=req.find("\r\n\r\n");
    if(headEnd!=std::string::npos)break;
  }
  if(headEnd==std::string::npos){respond(client,400,"Invalid request");return;}
  const auto first=req.find("\r\n");
  if(first==std::string::npos){respond(client,400,"Invalid request");return;}
  const std::string line=req.substr(0,first);
  if(line=="GET / HTTP/1.1"||line=="GET / HTTP/1.0") {
    respond(client,200,html);return;
  }
  if(line!="POST /submit HTTP/1.1"&&line!="POST /submit HTTP/1.0"){
    respond(client,line.rfind("POST ",0)==0?404:405,"Unsupported request");return;
  }
  size_t length=static_cast<size_t>(-1);
  bool form=false;
  size_t at=first+2;
  while(at<headEnd) {
    const size_t end=req.find("\r\n",at);
    if(end==std::string::npos||end>headEnd)break;
    const std::string h=lower(req.substr(at,end-at));
    if(h.rfind("content-length:",0)==0){
      const std::string value=trim(h.substr(15));
      if(value.empty()||value.find_first_not_of("0123456789")!=std::string::npos){respond(client,400,"Bad size");return;}
      length=std::strtoul(value.c_str(),nullptr,10);
    }
    if(h.rfind("content-type:",0)==0&&h.find("application/x-www-form-urlencoded")!=std::string::npos)form=true;
    at=end+2;
  }
  if(length==static_cast<size_t>(-1)||!form){respond(client,400,"Expected form");return;}
  if(length>4096){respond(client,413,"Link too long");return;}
  std::string body=req.substr(headEnd+4);
  while(body.size()<length){
    const auto n=recv(client,buffer,std::min(sizeof(buffer),length-body.size()),0);
    if(n<=0)break;
    body.append(buffer,static_cast<size_t>(n));
  }
  if(body.size()<length){respond(client,400,"Truncated form");return;}
  body.resize(length);
  std::string url;
  for(size_t pos=0;pos<body.size();){
    const auto end=body.find('&',pos);
    const auto part=body.substr(pos,end==std::string::npos?std::string::npos:end-pos);
    if(part.rfind("url=",0)==0){url=decode(part.substr(4));break;}
    if(end==std::string::npos)break;
    pos=end+1;
  }
  if(url.size()>2048||url.empty()) {respond(client,400,"Invalid URL size");return;}
  try { (void)Url::parse(url); }
  catch(...) {respond(client,400,"URL must use HTTP or HTTPS");return;}
  // Pending is staged by the HTTP thread, not queued for download.
  // Dedicated setter is private; use the public friend-like stage method.
  if(!server.stage(url)){respond(client,409,"A link is awaiting PS4 approval");return;}
  respond(client,200,sent);
}
}
PhoneShareServer::~PhoneShareServer(){
  stop_=true;
  if(listener>=0)shutdown(listener,SHUT_RDWR);
  if(worker.joinable())worker.join();
  if(listener>=0)close(listener);
}
void PhoneShareServer::start(const std::string& ip,unsigned short port){
  if(worker.joinable())throw std::runtime_error("Phone sharing already started");
  localIp_=ip;port_=port;stop_=false;
  const int fd=socket(AF_INET,SOCK_STREAM,0);
  if(fd<0)throw std::runtime_error("Phone sharing socket failed");
  sockaddr_in addr{};
  addr.sin_family=AF_INET;addr.sin_port=htons(port);
  if(inet_pton(AF_INET,ip.c_str(),&addr.sin_addr)!=1){close(fd);throw std::runtime_error("Invalid PS4 LAN IP");}
  int reuse=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
  if(bind(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0){close(fd);throw std::runtime_error("Phone sharing bind failed: check PS4 IP");}
  if(listen(fd,2)!=0){close(fd);throw std::runtime_error("Phone sharing listen failed");}
  listener=fd;ready=true;
  try { worker=std::thread([this]{serve();}); }
  catch(...){ready=false;close(fd);listener=-1;throw;}
}
bool PhoneShareServer::stage(const std::string& url) {
  std::lock_guard<std::mutex> lk(gate);
  if(!pendingUrl_.empty())return false;
  pendingUrl_=url;return true;
}
std::string PhoneShareServer::pending() const {
  std::lock_guard<std::mutex> lk(gate);return pendingUrl_;
}
std::string PhoneShareServer::takePending() {
  std::lock_guard<std::mutex> lk(gate);
  auto url=pendingUrl_;pendingUrl_.clear();return url;
}
void PhoneShareServer::rejectPending(){std::lock_guard<std::mutex> lk(gate);pendingUrl_.clear();}
std::string PhoneShareServer::error() const {std::lock_guard<std::mutex> lk(gate);return lastError_;}
void PhoneShareServer::serve(){
  while(!stop_){
    fd_set fds;FD_ZERO(&fds);FD_SET(listener,&fds);
    timeval tv{0,250000};
    int rc=select(listener+1,&fds,nullptr,nullptr,&tv);
    if(rc<=0)continue;
    sockaddr_in peer{};socklen_t len=sizeof(peer);
    const int client=accept(listener,reinterpret_cast<sockaddr*>(&peer),&len);
    if(client<0)continue;
    try{handleClient(*this,client);}catch(...){respond(client,400,"Invalid request");}
    close(client);
  }
  ready=false;
}
}
