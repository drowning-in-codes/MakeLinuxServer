#pragma once

#include "Channel.hpp"
#include "utils/log.hpp"
#include <functional>
#include <memory>
#include <utils/utils.hpp>
class Buffer;
class Connection {
  // 创建连接
public:
  DISALLOW_COPY_AND_MOVE(Connection);
  Connection(EventLoop *_loop, int fd, int t_connid);
  Connection(EventLoop *_loop, SerSocket *t_socket, int t_connid)
      : Connection(_loop, t_socket->get_fd(), t_connid) {}
  enum ConnectionState {
    Invalid = 1,
    Connected,
    Closed,
  };
  // void echo(int sockfd);
  ~Connection();
  void Read();
  void Write();
  void HandleClose();
  void HandleMessage();

  // setter and getter
  void setCloseCallback(const std::function<void(int)> &callback);
  void SetOnConnectCallback(std::function<void(Connection *)> const &callback);
  int getFd() const { return fd; }
  void setFd(int t_fd) { fd = t_fd; }
  void SetSendBuffer(Buffer *t_send_buffer);
  void SetReadBuffer(Buffer *t_read_buffer);
  void SetSendBuffer(const char *data);
  void SetReadBuffer(const char *data);
  ConnectionState GetState() const { return m_state; }

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

private:
  int conid;
  void ReadNonBlocking();
  void WriteNonBlocking();
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
  std::function<void(int)> closeCallback;
  std::function<void(Connection *)> onMessage;
};