#include "file_utils.hpp"
#include "http_server.hpp"
#include "io_context.hpp"
#include "reflect.hpp"
#include <unistd.h>

using namespace std::chrono_literals;

// C++ 全栈，聊天服务器
// 1. AJAX，轮询与长轮询 (OK)
// 2. WebSocket，JSON 消息

struct Message {
  std::string user;
  std::string content;

  REFLECT(user, content);
};

struct RecvParams {
  uint32_t first;

  REFLECT(first);
};

std::vector<Message> messages;
stop_source recv_timeout_stop = stop_source::make();
