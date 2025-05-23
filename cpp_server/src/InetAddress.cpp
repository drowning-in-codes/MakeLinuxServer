#include <InetAddress.hpp>
#include <arpa/inet.h>
#include <stdexcept>
InetAddress::InetAddress(std::string_view ip, uint16_t port) {
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip.data(), &addr.sin_addr) <= 0) {
    throw std::runtime_error("Invalid IP address");
  }
  addr_len = sizeof(addr);
}

