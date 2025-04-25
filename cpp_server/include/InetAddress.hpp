#pragma once
#include <arpa/inet.h>
#include <cstdint>
#include <string>
#include <sys/socket.h>
/**
    IPV4 address
 */
class InetAddress {
public:
  struct sockaddr_in addr;
  socklen_t addr_len;
  InetAddress(sockaddr_in &t_addr) : addr(t_addr), addr_len(sizeof(addr)){};
  InetAddress() : addr_len(sizeof(addr)) {
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
  }
  InetAddress(const char *ip, uint16_t port);
  uint16_t getPort() const { return ntohs(addr.sin_port); }
  std::string getIp() const {
    char *ip = new char[32];
    inet_ntop(AF_INET, &addr.sin_addr, ip, addr_len);
    std::string ip_str(ip);
    delete[] ip;
    return ip_str;
  }
};