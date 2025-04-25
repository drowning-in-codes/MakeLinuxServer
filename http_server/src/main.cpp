#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <coroutine>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fmt/core.h>
#include <iostream>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <ratio>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>
struct Task {
  struct promise_type {
    Task get_return_object() { return Task(handle_type::from_promise(*this)); }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void unhandled_exception() { std::terminate(); }
    void return_void() {}
  };
  using handle_type = std::coroutine_handle<promise_type>;

  handle_type coro;

  Task(handle_type h) : coro(h) {}
  ~Task() {
    if (coro)
      coro.destroy();
  }

  void resume() {
    if (!coro.done())
      coro.resume();
  }
};
int check_error(const char *msg, int res) {
  if (res == -1) {
    fmt::print("{}:{}", msg, strerror(errno));
    throw;
  }
  return res;
}
template <typename T> T check_error(const char *msg, T res) {
  if (res == -1) {
    fmt::print("{}:{}", msg, strerror(errno));
    auto ec = std::error_code(errno, std::system_category());
    throw std::system_error(ec, msg);
  }
  return res;
}
#define CHECK_CALL(func, ...) check_error(#func, func(__VA_ARGS__))
struct socket_address_fatpr {
  sockaddr *m_addr;
  std::size_t m_addr_len;
};
struct socket_address_storage {
  union {
    sockaddr m_addr;
    sockaddr_storage m_addr_storage;
  };
  socklen_t m_addr_len;
  operator socket_address_fatpr() { return {&m_addr, m_addr_len}; }
};
struct address_resolved_entry {
  addrinfo *m_curr;
  socket_address_fatpr get_address() const {
    return {m_curr->ai_addr, m_curr->ai_addrlen};
  }
  int create_socket() const {
    int sockfd =
        socket(m_curr->ai_family, m_curr->ai_socktype, m_curr->ai_protocol);
    if (sockfd == -1) {
      fmt::print("create socket failed: {},{}\n", errno, strerror(errno));
      throw;
    }
    return sockfd;
  }
  int create_socket_and_bind() const {
    int sockfd =
        socket(m_curr->ai_family, m_curr->ai_socktype, m_curr->ai_protocol);
    if (sockfd == -1) {
      fmt::print("create socket failed: {},{}\n", errno, strerror(errno));
      throw;
    }
    CHECK_CALL(bind, sockfd, m_curr->ai_addr, m_curr->ai_addrlen);
    return sockfd;
  }
};
using StringMap = std::map<std::string, std::string>;
struct http11_header_parser {
  std::string m_header;       // GET / HTTP/1.1 Host:xxx Keep-connection: alive
  std::string m_heading_line; // "GET / HTTP/1.1"
  std::string m_body;         // 多读的消息
  StringMap m_header_keys;    //{Host:xxx,...}
  bool m_header_finished{false};
  [[nodiscard]] bool header_finished() { return m_header_finished; }
  void _extract_heading_line() {
    size_t pos = m_header.find("\r\n");
    m_heading_line = m_header.substr(0, pos);
  }
  std::string _headline_first() {
    auto &headline = m_heading_line;
    size_t pos = headline.find(" ");
    std::string t_method;
    if (pos == std::string::npos) {
      // throw std::runtime_error("请求错误");
      return "GET";
    }
    //找到第一个空格 ->方法
    return headline.substr(0, pos);
  }

  std::string _headline_second() {
    auto &headline = m_heading_line;
    size_t first_pos = headline.find(" ");
    std::string t_method;
    if (first_pos == std::string::npos) {
      return "";
    }
    size_t second_pos = headline.find(" ", first_pos);
    if (second_pos == std::string::npos) {
      // 找不到第二个空格
      return "";
    }
    size_t url_len = second_pos - first_pos - 1;
    return headline.substr(first_pos + 1, url_len);
  }
  std::string _headline_third() {
    // request GET / HTTP/1.1
    // response HTTP/1.1 200 OK
    auto &headline = m_heading_line;
    size_t first_pos = headline.find(" ");
    std::string t_method;
    if (first_pos == std::string::npos) {
      return "";
    }
    size_t second_pos = headline.find(" ", first_pos);
    if (second_pos == std::string::npos) {
      // 找不到第二个空格
      return "";
    }
    size_t headline_end_pos = headline.find("\r\n");
    size_t version_len = headline_end_pos - second_pos - 1;
    return headline.substr(second_pos + 1, version_len);
  }
  void push_chunk(std::string_view chunk) {
    if (!m_header_finished) {
      m_header.append(chunk);
      size_t pos = m_header.find("\r\n\r\n");
      if (pos != std::string::npos) {
        // 如果匹配到头部结束,头部结束
        m_header_finished = true;
        // 截取多读的正文
        std::string extra_body = m_header.substr(pos + 4);
        m_body.append(extra_body);
        m_header.resize(pos);

        // 提取头部
        this->_extract_heading_line();
        this->_extract_headers();
      } else {
        // 没有截至 继续都
        m_header.append(chunk);
      }
    }
  }
  void _extract_headers() {
    size_t pos = m_header.find("\r\n");
    while (pos != std::string::npos) {
      // 如果找到了字段
      pos += 2;
      size_t next_pos = m_header.find("\r\n", pos);
      size_t line_len = std::string::npos;
      if (next_pos != std::string::npos) {
        line_len = next_pos - pos;
      }
      std::string line = m_header.substr(pos, line_len);
      size_t colon = line.find(": ");
      if (colon != std::string::npos) {
        // 找到字段
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 2);
        // 统一小写
        std::transform(key.begin(), key.end(), key.begin(),
                       [](char c) { return std::tolower(c); });
        m_header_keys.insert({key, val});
      }
      pos = m_header.find("\r\n", pos);
    }
  }
  StringMap &headers() { return m_header_keys; }
  std::string &heading_line() { return m_heading_line; }
  std::string &extra_body() { return m_body; }
};

template <typename HeaderParer = http11_header_parser> struct http_base_parser {
  HeaderParer m_header_parser;
  size_t m_content_length{};
  bool m_body_finished = false;
};
template <typename HeaderParer = http11_header_parser>
struct http_request_parser {
  HeaderParer m_header_parser;
  size_t m_content_length{};
  bool m_body_finished = false;
  [[nodiscard]] bool need_more_chunk() const { return !m_body_finished; }

  void push_chunk(std::string_view chunk) {
    if (!m_header_parser.header_finished()) {
      // 头部还没结束
      m_header_parser.push_chunk(chunk);
      if (!m_header_parser.header_finished()) {
        m_content_length = 0;
      } else {
        m_content_length = _extract_content_length();
        if (m_header_parser.extra_body().size() >= m_content_length) {
          m_body_finished = true;
          body().resize(m_content_length);
        }
      }
    } else {
      body().append(chunk);
      m_content_length = _extract_content_length();
      if (m_header_parser.extra_body().size() >= m_content_length) {
        m_body_finished = true;
        body().resize(m_content_length);
      }
    }
  }
  size_t _extract_content_length() {
    auto &headers = m_header_parser.headers();
    auto it = headers.find("content-length");
    if (it == headers.end()) {
      // 没找到
      return 0;
    }
    size_t val;
    auto [ptr, ec] = std::from_chars(
        it->second.c_str(), it->second.c_str() + it->second.size(), val);
    if (ec != std::errc()) {
      std::cout << "转换失败\n";
    }
    std::cout << "content-length:" << val << '\n';
    return val;
  }
  std::string &body() { return m_header_parser.extra_body(); }
  std::string &header_raw() { return m_header_parser.m_header; }
  std::string &header_line() { return m_header_parser.heading_line(); }
  StringMap &header_keys() { return m_header_parser.headers(); }
};
struct address_resolver {
  addrinfo *m_head = nullptr;

  address_resolved_entry resolve(std::string const &name,
                                 std::string const &service) {
    addrinfo *addrinfo;
    int err = getaddrinfo(name.c_str(), service.c_str(), NULL, &addrinfo);
    if (err != 0) {
      fmt::print("getaddrinofo: {},{}\n", err, gai_strerror(err));
      throw;
    }
    return {addrinfo};
  }
  address_resolved_entry get_first_entry() { return {m_head}; }
  address_resolver() = default;
  address_resolver(address_resolver &&t_address_resolver)
      : m_head(t_address_resolver.m_head) {
    t_address_resolver.m_head = nullptr;
  }
  ~address_resolver() {
    if (m_head) {
      freeaddrinfo(m_head);
    }
  }
};

int main() {
  setlocale(LC_ALL, "zh_CN.UTF-8");
  address_resolver resovler;
  auto entry = resovler.resolve("127.0.0.1", "8080");
  int listenfd = entry.create_socket_and_bind();
  CHECK_CALL(listen, listenfd, SOMAXCONN);
  std::vector<std::thread> pool;
  while (true) {
    socket_address_storage addr;
    int connid = CHECK_CALL(accept, listenfd, &addr.m_addr, &addr.m_addr_len);
    pool.emplace_back([connid]() {
      char buf[1024]{};
      http_request_parser req_parse;
      do {
        size_t n = CHECK_CALL(read, connid, buf, sizeof(buf));
        req_parse.push_chunk(std::string_view(buf, n));

      } while (req_parse.need_more_chunk());
      // ssize_t n = CHECK_CALL(read, connid, buf, sizeof(buf));
      // auto req = std::string(buf, n);
      fmt::print("收到请求头部: {}\n", req_parse.header_raw());
      fmt::print("收到请求正文: {}\n", req_parse.body());
      std::string resp = "HTTP/1.1 200 ok\r\nServer: proanimer\r\nConnection: "
                         "close\r\nContent-length: " +
                         std::to_string(req_parse.body().size()) + "\r\n\r\n" +
                         req_parse.body();
      CHECK_CALL(write, connid, resp.data(), resp.size());
      close(connid);
    });
  }

  for (auto &t : pool) {
    t.join();
  }
  return 0;
}
