#pragma once

#include "Channel.hpp"
#include "utils/log.hpp"
#include <functional>
#include <memory>
#include <utils/utils.hpp>
class Buffer;
class Connection : public std::enable_shared_from_this<Connection> {
  // 创建连接
public:
  DISALLOW_COPY_AND_MOVE(Connection);
  Connection(EventLoop *_loop, int fd, int t_connid);
  Connection(EventLoop *_loop, SerSocket *t_socket, int t_connid)
      : Connection(_loop, t_socket->get_fd(), t_connid) {}
  enum class ConnectionState {
    Invalid = 1,
    Connected,
    Closed,
  };
  // void echo(int sockfd);
  ~Connection();
  void Read();
  void Write();

  void ConnectionEstablished() {
    m_state = ConnectionState::Connected;
    // 增加引用计数
    m_channel->Tie(shared_from_this());
    if (onConnectinoCallback) {
      onConnectinoCallback(shared_from_this());
    }
  }
  // 消息和关闭处理方法
  void HandleClose();
  void HandleMessage();
  bool isBlocking() const {
    // F_GETFL
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
      throw std::runtime_error("Failed to get socket flags");
    }
    return (flags & O_NONBLOCK) == 0;
  }
  void setBlocking(bool blocking) { ::setBlock(fd, blocking); }
  // setter and getter
  int getFd() const { return fd; }
  void setFd(int t_fd) { fd = t_fd; }
  void SetSendBuffer(Buffer *t_send_buffer);
  void SetReadBuffer(Buffer *t_read_buffer);
  void SetSendBuffer(const char *data);
  void SetReadBuffer(const char *data);
  ConnectionState GetState() const { return m_state; }

  void ConnectionDestructor();
  // 设置发送缓冲区并发送数据
  void Send(const std::string &data);
  void Send(const char *data);
  Buffer *GetSendBuffer() const { return send_buffer.get(); }
  Buffer *GetReadBuffer() const { return read_buffer.get(); }
  const char *GetSendData() const;
  const char *GetReadData() const;
  void GetlineSendBuffer();
  int getConid() const { return conid; }
  EventLoop *getEventLoop() const { return m_loop; }
  Channel *getChannel() const { return m_channel.get(); }

  // 设置连接时,消息处理时以及关闭回调函数
  void setOnMessageCallback(
      const std::function<void(const std::shared_ptr<Connection> &)>
          &callback) {
    onMessageCallback = std::move(callback);
  }
  void SetOnConnectCallback(
      const std::function<void(const std::shared_ptr<Connection> &)>
          &callback) {
    onConnectinoCallback = std::move(callback);
  }
  void setOnCloseCallback(
      const std::function<void(const std::shared_ptr<Connection> &)>
          &callback) {
    closeCallback = std::move(callback);
  }

private:
  int conid;
  void ReadNonBlocking();
  void ReadBlocking();
  void WriteNonBlocking();
  void WriteBlocking();
  ConnectionState m_state;
  const static int BUFFER_SIZE = 1024;
  std::unique_ptr<Logger> logger;
  std::unique_ptr<Buffer> read_buffer;
  std::unique_ptr<Buffer> send_buffer;
  void send(int fd);
  int fd;
  //   InetAddress *addr;
  EventLoop *m_loop;                  // 包含epoll
  std::unique_ptr<Channel> m_channel; //  包含epoll_data
  std::function<void(const std::shared_ptr<Connection> &)> onConnectinoCallback;
  std::function<void(const std::shared_ptr<Connection> &)> onMessageCallback;
  std::function<void(const std::shared_ptr<Connection> &)> closeCallback;
};