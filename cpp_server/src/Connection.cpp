#include "utils/log.hpp"
#include <Buffer.hpp>
#include <Channel.hpp>
#include <Connection.hpp>
#include <EventLoop.hpp>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <unistd.h>
Connection::Connection(EventLoop *_loop, int t_fd, int connid)
    : m_loop(_loop), fd(t_fd), conid(connid) {
  assert(fd >= 0);
  logger = std::make_unique<ConsoleLogger>();
  if (m_loop != nullptr) {
    // 是否有传入的eventloop,如果传入了eventloop,表示会使用对应的loop添加epoll事件
    m_channel = std::make_unique<Channel>(m_loop, fd);
    // 加入epll实例
    m_channel->enableET();
    m_channel->setReadCallback([this]() {
      Read();
      // 处理读事件
      if (onMessage) {
        onMessage(this);
      }
    });
    // 更新epll实例
    m_channel->enableRead();
  }
  read_buffer = std::make_unique<Buffer>();
  send_buffer = std::make_unique<Buffer>();
  m_state = ConnectionState::Connected;
  // logger->logInfo("Connection established with fd: " + std::to_string(fd));
  // m_channel->setWriteCallback([this]() { Write(); });
  // m_channel->setUseET(true);
  // m_channel->setUseThreadPool(true);
}
void Connection::SetReadBuffer(const char *data) { send_buffer->setBuf(data); }
void Connection::SetSendBuffer(const char *data) { send_buffer->setBuf(data); }
void Connection::SetSendBuffer(Buffer *t_send_buffer) {
  send_buffer.reset(t_send_buffer);
}
void Connection::SetReadBuffer(Buffer *t_read_buffer) {
  read_buffer.reset(t_read_buffer);
}

void Connection::Send(const std::string &data) {
  send_buffer->setBuf(data.c_str());
  Write();
}
void Connection::Send(const char *data) {
  send_buffer->setBuf(data);
  Write();
}

// 设置连接的回调函数 包括fd可读和关闭连接
void Connection::SetOnConnectCallback(
    std::function<void(Connection *)> const &callback) {
  onMessage = std::move(callback);
}
void Connection::setCloseCallback(const std::function<void(int)> &callback) {
  closeCallback = std::move(callback);
}

const char *Connection::GetSendData() const { return send_buffer->data(); }
const char *Connection::GetReadData() const { return read_buffer->data(); }
void Connection::GetlineSendBuffer() {
  send_buffer->getline();
  m_channel->enableWrite();
}
void Connection::Read() {
  if (m_state != ConnectionState::Connected) {
    return;
  }
  read_buffer->clear();
  if (isBlocking()) {
    ReadBlocking();
  } else {
    ReadNonBlocking();
  }
}

void Connection::Write() {
  if (m_state != ConnectionState::Connected) {
    return;
  }
  if (isBlocking()) {
    WriteBlocking();
  } else {
    WriteNonBlocking();
  }
  send_buffer->clear();
}
void Connection::HandleClose() {
  if (m_state != ConnectionState::Closed) {
    m_state = ConnectionState::Closed;
    if (closeCallback) {
      closeCallback(fd);
    }
  }
}

void Connection::ReadBlocking() {
  char data[BUFFER_SIZE];
  bzero(data, sizeof(data));
  bzero(data, sizeof(data));
  int intrTimes = 0;
  while (true && intrTimes < 3) {
    ssize_t bytesRead = read(fd, data, BUFFER_SIZE);
    if (bytesRead > 0) {
      // 处理读取到的数据
      logger->logInfo("Received data: " + std::string(data, bytesRead));
      read_buffer->append(data, bytesRead);
      break;
    } else if (bytesRead == -1 && errno == EINTR) { //客户端正常中断、继续读取
      printf("continue reading");
      intrTimes++;
      continue;
    } else if (bytesRead == 0) {
      // 客户端关闭连接
      printf("Client disconnected\n");
      HandleClose();
      break;

    } else {
      perror("read");
      HandleClose();
      break;
    }
  }
}
void Connection::ReadNonBlocking() {
  char data[BUFFER_SIZE];
  while (true) {
    bzero(data, sizeof(data));
    printf("blocking read\n");
    ssize_t bytesRead = read(fd, data, BUFFER_SIZE);
    printf("bytesRead: %ld\n", bytesRead);
    if (bytesRead > 0) {
      // 处理读取到的数据
      logger->logInfo("Received data: " + std::string(data, bytesRead));
      read_buffer->append(data, bytesRead);
    } else if (bytesRead == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // 非阻塞读取,等待数据
      break;
    } else if (bytesRead == -1 && errno == EINTR) { //客户端正常中断、继续读取
      printf("continue reading");
      continue;
    } else if (bytesRead == 0) {
      // 客户端关闭连接
      printf("Client disconnected\n");
      HandleClose();
      break;
    } else {
      perror("read");
      HandleClose();
      break;
    }
  }
}
void Connection::WriteBlocking() {
  size_t totalBytes = send_buffer->size();
  size_t bytesSent = 0;
  int intrTimes = 0;
  while (bytesSent < totalBytes && intrTimes < 3) {
    ssize_t result =
        ::write(fd, send_buffer->data() + bytesSent, totalBytes - bytesSent);
    if (result == -1) {
      if (errno == EINTR) {
        // Interrupted by a signal, continue sending
        intrTimes++;
        continue;
      }
      // other errors
      perror("other error when writing");
      m_state = ConnectionState::Closed;
      break;
    }
    bytesSent += result;
  }
}
void Connection::WriteNonBlocking() {
  size_t totalBytes = send_buffer->size();
  size_t bytesSent = 0;
  while (bytesSent < totalBytes) {
    ssize_t result =
        ::write(fd, send_buffer->data() + bytesSent, totalBytes - bytesSent);
    if (result == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Socket is non-blocking and no more data can be sent right now
        break;
      }
      if (errno == EINTR) {
        // Interrupted by a signal, continue sending
        continue;
      }
      // other errors
      perror("other error when writing");
      m_state = ConnectionState::Closed;
      break;
    }
    bytesSent += result;
  }
}
Connection::~Connection() { close(fd); }
