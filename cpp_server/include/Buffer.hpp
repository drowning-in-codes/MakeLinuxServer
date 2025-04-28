#pragma once
#include <cstring>
#include <iostream>
class Buffer {

private:
  std::string buffer;

public:
  const char *data() { return buffer.c_str(); }
  size_t size() { return buffer.size(); }
  void append(const char *data, size_t size) { buffer.append(data, size); }
  void append(const std::string &buf, size_t size) { buffer.append(buf); }
  void clear() { buffer.clear(); }
  void setBuf(const char *data) { buffer.assign(data); }
  void setBuf(const std::string &buf) { buffer = buf; }
  void resize(size_t size) { buffer.resize(size); }
  void getline() {
    buffer.clear();
    std::getline(std::cin, buffer);
  }
};