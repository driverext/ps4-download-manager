#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace dm {
// LAN-only input bridge. Incoming requests never trigger downloads.
// Every URL requires explicit acceptance using the PS4 controller.
class PhoneShareServer {
public:
  PhoneShareServer() = default;
  ~PhoneShareServer();
  PhoneShareServer(const PhoneShareServer&) = delete;
  PhoneShareServer& operator=(const PhoneShareServer&) = delete;
  void start(const std::string& localIp="192.168.1.150", unsigned short port=8787);
  bool running() const { return ready.load(); }
  bool stage(const std::string& url); // stores one incoming URL, never downloads
  std::string pending() const;
  std::string takePending();
  void rejectPending();
  std::string error() const;
private:
  void serve();
  mutable std::mutex gate;
  std::string localIp_;
  unsigned short port_=8787;
  std::string pendingUrl_;
  std::string lastError_;
  std::atomic<bool> stop_{false}, ready{false};
  std::thread worker;
  int listener=-1;
};
