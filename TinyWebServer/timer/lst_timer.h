#ifndef LST_TIMER
#define LST_TIMER
// 定时器处理非活动连接
// ===============
// 由于非活跃连接占用了连接资源，严重影响服务器的性能，通过实现一个服务器定时器，处理这种非活跃连接，释放连接资源。利用alarm函数周期性地触发SIGALRM信号,该信号的信号处理函数利用管道通知主循环执行定时器链表上的定时任务.
// > * 统一事件源
// > * 基于升序链表的定时器
// > * 处理非活动连接

#include <netinet/in.h>
class util_timer;

struct client_data {
  sockaddr_in address;
  int sockfd;
  util_timer *timer;
};

class util_timer {
public:
  util_timer() : prev(nullptr), next(nullptr) {}

  time_t expire;
  void (*cb_func)(client_data *);

  client_data *user_data;
  util_timer *prev;
  util_timer *next;
};

class sort_timer_lst {
public:
  sort_timer_lst();
  ~sort_timer_lst();
  void add_timer(util_timer *timer);
  void adjust_timer(util_timer *timer);

  void del_timer(util_timer *timer);
  void tick();

private:
  void add_timer(util_timer *timer, util_timer *lst_head);
  util_timer *head;
  util_timer *tail;
};

class Utils {

public:
  Utils() {}
  ~Utils() {}

  void init(int timeslot);

  // 设置文件非阻塞
  int setnonblocking(int fd);

  // 将文件描述符放入内核事件表注册
  void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

  // 信号处理函数
  static void sig_handler(int sig);
  // 设置信号函数
  void addsig(int sig, void(handler)(int), bool restart = true);
  //定时处理任务,重新定时以不断触发SIGALRM信号
  void timer_handler();
  void show_error(int connfd, const char *info);

public:
  static int *u_pipefd;
  sort_timer_lst m_timer_lst;
  static int u_epollfd;
  int m_TIMESLOT;
};
void cb_func(client_data *user_data);

#endif