#include "lst_timer.h"
#include "../http/http_conn.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/wait.h>
sort_timer_lst::sort_timer_lst() {
  head = NULL;
  tail = NULL;
}

sort_timer_lst::~sort_timer_lst() {
  util_timer *tmp = head;
  while (tmp) {
    head = tmp->next;
    delete tmp;
    tmp = head;
  }
}

void sort_timer_lst::add_timer(util_timer *timer) {
  if (!timer) {
    return;
  }
  if (!head) {
    head = tail = timer;
    return;
  }
  if (timer->expire < head->expire) {
    // 加入的timer的截止时间小于头的截至时间
    timer->next = head;
    head->prev = timer;
    head = timer;
    return;
  }
  // 如果加入的定时器截至时间大于头,则加入
  add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer *timer) {
  if (!timer) {
    return;
  }
  util_timer *tmp = timer->next;
  if (!tmp || (timer->expire < tmp->expire)) {
    return;
  }
  if (timer == head) {
    head = head->next;
    head->prev = NULL;
    timer->next = NULL;
    add_timer(timer, head);
  } else {
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    add_timer(timer, timer->next);
  }
}

void sort_timer_lst::del_timer(util_timer *timer) {
  if (!timer) {
    return;
  }
  if ((timer == head) && (timer == tail)) {
    delete timer;
    head = tail = NULL;
    return;
  }
  if (timer == head) {
    head = head->next;
    head->prev = NULL;
    delete timer;
    return;
  }
  if (timer == tail) {
    tail = tail->prev;
    tail->next = NULL;
    delete timer;
    return;
  }
  timer->prev->next = timer->next;
  timer->next->prev = timer->prev;
  delete timer;
  return;
}

void sort_timer_lst::tick() {
  if (!head) {
    return;
  }
  time_t cur = time(NULL);
  util_timer *tmp = head;
  while (tmp) {
    if (cur < tmp->expire) {
      // 在截止期内
      break;
    }
    // 超时 执行回调
    tmp->cb_func(tmp->user_data);
    head = tmp->next;
    if (head) {
      head->prev = NULL;
    }
    delete tmp;
    tmp = head;
  }
}

void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head) {
  util_timer *prev = lst_head;
  util_timer *tmp = prev->next;
  while (tmp) {
    if (timer->expire < tmp->expire) {
      prev->next = timer;
      timer->next = tmp;
      tmp->prev = timer;
      timer->prev = prev;
      break;
    }
    prev = tmp;
    tmp = tmp->next;
  }
  if (!tmp) {
    // tmp为空
    // 没有找到截止日期大于timer的,将timer插入到末尾
    prev->next = timer;
    timer->prev = prev;
    timer->next = NULL;
    tail = timer;
  }
}
void Utils::init(int timeslot) { m_TIMESLOT = timeslot; }

int Utils::setnonblocking(int fd) {
  //对文件描述符设置非阻塞
  int flags = fcntl(fd, F_GETFL);
  if (flags == -1) {
    perror("fcntl F_GETFL failed");
    return -1;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl F_SETFL failed");
    return -1;
  }
  return flags;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
  epoll_event event;
  event.data.fd = fd;
  if (1 == TRIGMode) {
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
  } else {
    event.events = EPOLLIN | EPOLLET;
  }
  if (one_shot) {
    event.events |= EPOLLONESHOT;
  }
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  setnonblocking(fd);
}

// 信号处理函数
void Utils::sig_handler(int sig) {
  int save_errno = errno;
  int msg = sig;
  send(u_pipefd[1], (char *)&msg, 1, 0);
  errno = save_errno;
}

void Utils::addsig(int sig, void(handler)(int), bool restart) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  if (restart) {
    // 如果信号中断了某个可重启的系统调用（如 read, write
    // 等），则自动重启该系统调用
    sa.sa_flags |= SA_RESTART;
  }
  // 在信号处理器中 屏蔽所有信号
  sigfillset(&sa.sa_mask);
  // 设置信号处理器
  assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler() {
  //执行回调
  m_timer_lst.tick();

  // 重新定时
  alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info) {
  send(connfd, info, strlen(info), 0);
  close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data *user_data) {
  epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
  assert(user_data);
  close(user_data->sockfd);
  http_conn::m_user_count--;
}