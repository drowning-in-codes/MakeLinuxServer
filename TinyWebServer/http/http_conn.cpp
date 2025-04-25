// http连接处理类
// ===============
// 根据状态转移,通过主从状态机封装了http连接类。其中,主状态机在内部调用从状态机,从状态机将处理状态和数据传给主状态机
// > * 客户端发出http连接请求
// > * 从状态机读取数据,更新自身状态和接收数据,传给主状态机
// > * 主状态机根据从状态机状态,更新自身状态,决定响应请求还是继续读取
#include "http_conn.h"

#include <asm-generic/errno.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mysql/mysql.h>
#include <strings.h>
#include <sys/uio.h>

// 定义http响应信息

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form =
    "Your request has bad syntax or is inherently impossible to staisfy.\n";

const char *error_403_title = "Forbidden";
const char *error_403_form =
    "You do not have permission to get file form this server.\n";

const char *error_404_title = "Not Found";
const char *error_404_form =
    "The requested file was not found on this server.\n";

const char *error_500_title = "Internal Error";
const char *error_500_form =
    "There was an unusual problem serving the request file.\n";

locker m_lock;
std::map<std::string, std::string> users;

void http_conn::initmysql_result(connection_pool *connPool) {
  // 先从连接池中取出一个连接
  MYSQL *mysql = NULL;
  connectionRAII mysqlcon(&mysql, connPool);
  //在user表中检索username，passwd数据，浏览器端输入
  if (mysql_query(mysql, "SELECT username,passwd FROM user")) {
    LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
  }
  //从表中检索完整的结果集
  MYSQL_RES *result = mysql_store_result(mysql);

  //返回结果集中的列数
  int num_fields = mysql_num_fields(result);

  // 返回所有字段结构的数组
  MYSQL_FIELD *fields = mysql_fetch_fields(result);

  while (MYSQL_ROW row = mysql_fetch_row(result)) {
    std::string temp1{row[0]};
    std::string temp2{row[1]};
    users[temp1] = temp2;
  }
}

//对文件描述符设置非阻塞
int setnonblocking(int fd) {
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
  epoll_event event;
  event.data.fd = fd;

  if (1 == TRIGMode)
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
  else
    event.events = EPOLLIN | EPOLLRDHUP;

  if (one_shot)
    event.events |= EPOLLONESHOT;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  setnonblocking(fd);
}

//从内核时间表删除描述符
void removefd(int epollfd, int fd) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
  close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode) {
  epoll_event event;
  event.data.fd = fd;

  if (1 == TRIGMode)
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
  else
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

//关闭连接，关闭一个连接，客户总量减一
void http_conn::close_conn(bool real_close) {
  if (real_close && (m_sockfd != -1)) {
    printf("close %d\n", m_sockfd);
    removefd(m_epollfd, m_sockfd);
    m_sockfd = -1;
    m_user_count--;
  }
}

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root,
                     int TRIGMode, int close_log, std::string user,
                     std::string passwd, std::string sqlname) {
  m_sockfd = sockfd;
  m_address = addr;
  // 添加客户端连接到epoll实例中
  addfd(m_epollfd, sockfd, true, TRIGMode);
  m_user_count++;

  doc_root = root;
  m_TRIGMode = TRIGMode;
  m_close_log = close_log;
  strcpy(sql_user, user.c_str());
  strcpy(sql_passwd, passwd.c_str());
  strcpy(sql_name, sqlname.c_str());
  init();
}

void http_conn::init() {
  mysql = NULL;
  bytes_to_send = 0;
  bytes_have_send = 0;
  m_check_state = CHECK_STATE_REQUESTLINE;
  m_linger = false;
  m_method = METHOD::GET;
  m_url = 0;
  m_version = 0;
  m_content_length = 0;
  m_host = 0;
  m_start_line = 0;
  m_checked_idx = 0;
  m_read_idx = 0;
  m_write_idx = 0;
  cgi = 0;
  m_state = 0;
  timer_flag = 0;
  improv = 0;

  memset(m_read_buf, 0, READ_BUFFER_SIZE);
  memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
  memset(m_real_file, '\0', FILENAME_LEN);
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line() {
  // 爬取一行 将行末\r\n设置\0截止
  char temp;
  for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
    temp = m_read_buf[m_checked_idx];
    if (temp == '\r') {
      if ((m_checked_idx + 1) == m_read_idx) {
        return LINE_OPEN;
      } else if (m_read_buf[m_checked_idx + 1] == '\n') {
        m_read_buf[m_checked_idx++] = '\0';
        m_read_buf[m_checked_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    } else if (temp == '\n') {
      if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
        m_read_buf[m_checked_idx - 1] = '\0';
        m_read_buf[m_checked_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  return LINE_OPEN;
}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once() {
  if (m_read_idx >= READ_BUFFER_SIZE) {
    return false;
  }
  int bytes_read = 0;
  // LT读取数据 水平触发
  // 当状态变化后epoll_wait会一直返回值
  if (0 == m_TRIGMode) {
    bytes_read = recv(m_sockfd, m_read_buf + m_read_idx,
                      READ_BUFFER_SIZE - m_read_idx, 0);
    m_read_idx += bytes_read;
    if (bytes_read <= 0) {
      return false;
    }
    return true;
  } else {
    // ET读数据
    // 边缘触发
    while (true) {
      bytes_read = recv(m_sockfd, m_read_buf + m_read_idx,
                        READ_BUFFER_SIZE - m_read_idx, 0);
      if (bytes_read == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // 当尝试从一个非阻塞模式下的空套接字读取数据时，read() 会立即返回 -1
          // 并设置 errno 为 EAGAIN 或 EWOULDBLOCK，表示当前没有可用的数据可读
          // 还未读取到
          break;
        }
        return false;
      } else if (bytes_read == 0) {
        // 断开连接
        return false;
      }
      m_read_idx += bytes_read;
    }
    return true;
  }
}

//解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text) {
  //    GET / HTTP/1.1
  //    GET http://xxx HTTP/1.1
  m_url = strpbrk(text, " \t");
  if (!m_url) {
    return BAD_REQUEST;
  }
  *(m_url++) = '\0'; // 进入到第二个字段
  char *method = text;
  // 第一个空格
  if (strcasecmp(method, "GET") == 0) {
    m_method = GET;
  } else if (strcasecmp(method, "POST") == 0) {
    m_method = POST;
    cgi = 1;
  } else {
    return BAD_REQUEST;
  }
  m_url += strspn(m_url, " \t");
  m_version = strpbrk(m_url, " \t");
  if (!m_version) {
    return BAD_REQUEST;
  }
  *m_version++ = '\0';                   // 将url截至
  m_version += strspn(m_version, " \t"); // 跳过中间空白
  if (strcasecmp(m_version, "HTTP/1.1") != 0) {
    return BAD_REQUEST;
  }
  if (strncasecmp(m_url, "http://", 7) == 0) {
    m_url += 7;
    m_url = strchr(m_url, '/');
  }
  if (strncasecmp(m_url, "https://", 8) == 0) {
    m_url += 8;
    m_url = strchr(m_url, '/');
  }
  if (!m_url || m_url[0] != '/') {
    return BAD_REQUEST;
  }
  if (strlen(m_url) == 1) {
    strcat(m_url, "judge.html");
  }
  m_check_state = CHECK_STATE_HEADER;
  return NO_REQUEST;
}

//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text) {
  // host: xxxx
  // or connection: xxx\r\n
  // or content-length: xx\r\n
  if (text[0] == '\0') {
    // 请求头部和数据部分间的\r\n
    if (m_content_length != 0) {
      // content_length不为0,继续读数据部分
      m_check_state = CHECK_STATE_CONTENT;
      return NO_REQUEST;
    }
    return GET_REQUEST;
  } else if (strncasecmp(text, "Connection:", 11) == 0) {
    text += 11;
    text += strspn(text, " \t"); // 相当于跳过中间的空白部分
    if (strcasecmp(text, "keep-alive") == 0) {
      m_linger = true;
    }
  } else if (strncasecmp(text, "content-length:", 15) == 0) {
    text += 15;
    text += strspn(text, " \t"); // 相当于跳过中间的空白部分
    m_content_length = atol(text);
  } else if (strncasecmp(text, "host:", 5) == 0) {
    text += 5;
    text += strspn(text, " \t"); // 相当于跳过中间的空白部分
    m_host = text;
  } else {
    LOG_INFO("unknown header: %s", text);
  }
  return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text) {
  if (m_read_idx >= (m_content_length + m_checked_idx)) {
    // 读取的数据多余内容部分长度
    text[m_content_length] = '\0';
    // POST请求中最后为输入的用户名和密码
    m_string = text;
    return GET_REQUEST;
  }
  return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read() {
  LINE_STATUS line_status = LINE_OK;
  HTTP_CODE ret = NO_REQUEST;
  char *text = 0;
  while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
         ((line_status = parse_line()) == LINE_OK)) {
    text = get_line();
    m_start_line = m_checked_idx;
    LOG_INFO("%s", text);
    switch (m_check_state) {
    case CHECK_STATE_REQUESTLINE: {
      // 请求行
      ret = parse_request_line(text);
      if (ret == BAD_REQUEST) {
        return BAD_REQUEST;
      }
      break;
    }
    case CHECK_STATE_HEADER: {
      // 请求头
      ret = parse_headers(text);
      if (ret == GET_REQUEST) {
        return do_request();
      }
      break;
    }
    case CHECK_STATE_CONTENT: {
      ret = parse_content(text);
      if (ret == GET_REQUEST) {
        return do_request();
      } else {
        // 还未到结尾 继续读
        line_status = LINE_OPEN;
        break;
      }
    }
    default:
      return INTERNAL_ERROR;
    }
  }
  return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request() {
  strcpy(m_real_file, doc_root);
  int len = strlen(doc_root);
  // /path/2
  // /path/3
  const char *p = strrchr(m_url, '/');
  if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {
    // 登录或注册
    char flag = m_url[1];
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/");
    strcat(m_url_real, m_url + 2);
    strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
    free(m_url_real);

    char name[100], passwd[100];
    int i;
    // user=123&password=123
    for (i = 5; m_string[i] != '&'; ++i) {
      name[i - 5] = m_string[i];
    }
    name[i - 5] = '\0';
    int j = 0;
    for (i = i + 10; m_string[i] != '\0'; ++i, ++j) {
      passwd[j] = m_string[i];
    }
    passwd[j] = '\0';
    if (*(p + 1) == '3') {
      // 注册
      // 检测数据库是否有重名
      char *sql_insert = (char *)malloc(sizeof(char) * 200);
      strcpy(sql_insert, "INSERT INTO user(username,passwd) VALUES(");
      strcat(sql_insert, "'");
      strcat(sql_insert, name);
      strcat(sql_insert, "','");
      strcat(sql_insert, passwd);
      strcat(sql_insert, "')");

      if (users.find(name) == users.end()) {
        // 没找到
        m_lock.lock();
        int res = mysql_query(mysql, sql_insert);
        users.insert({name, passwd});
        m_lock.unlock();
        if (!res) {
          // 插入正常 res=0
          strcpy(m_url, "/log.html");
        } else {
          // 插入数据失败
          strcpy(m_url, "/registerError.html");
        }
      } else {
        // 找到 有重复号
        strcpy(m_url, "/registerError.html");
      }
    } else if (*(p + 1) == '2') {
      //登录
      if (users.find(name) != users.end() && users[name] == passwd) {
        strcpy(m_url, "/welcome.html");
      } else {
        // 用户不存在或密码错误
        strcpy(m_url, "/logError.html");
      }
    }
  }
  if (*(p + 1) == '0') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/register.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real) + 1);

    free(m_url_real);
  } else if (*(p + 1) == '1') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/log.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    free(m_url_real);
  } else if (*(p + 1) == '5') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/picture.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    free(m_url_real);
  } else if (*(p + 1) == '6') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/video.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    free(m_url_real);
  } else if (*(p + 1) == '7') {
    char *m_url_real = (char *)malloc(sizeof(char) * 200);
    strcpy(m_url_real, "/fans.html");
    strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

    free(m_url_real);
  } else {
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
  }

  if (stat(m_real_file, &m_file_stat) < 0) {
    return NO_RESOURCE;
  }
  if (!(m_file_stat.st_mode & S_IROTH)) {
    return FORBIDDEN_REQUEST;
  }
  if (S_ISDIR(m_file_stat.st_mode)) {
    return BAD_REQUEST;
  }
  int fd = open(m_real_file, O_RDONLY);
  //    映射到内存可读
  m_file_address =
      (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  return FILE_REQUEST;
}
void http_conn::unmap() {
  if (m_file_address) {
    munmap(m_file_address, m_file_stat.st_size);
    m_file_address = 0;
  }
}

bool http_conn::write() {
  int temp = 0;
  if (bytes_to_send == 0) {
    modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
    init();
    return true;
  }
  while (true) {
    // 写
    temp = writev(m_sockfd, m_iv, m_iv_count);
    if (temp < 0) {
      if (errno == EAGAIN) { // EWOULDBLOCK = EAGAIN
        // 缓冲区已满
        // 加入epoll实例
        modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
        return true;
      }
      //
      //将文件映射到的内存清理
      unmap();
      return false;
    }
    // 已写入
    bytes_have_send += temp;
    bytes_to_send -= temp;
    if (bytes_have_send >= m_iv[0].iov_len) {
      // 大于第一个缓冲区的大小(响应状态和头信息)
      // 第一个缓冲区大小设置为0
      // 第二个缓冲区修改base
      m_iv[0].iov_len = 0;
      m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
      m_iv[1].iov_len = bytes_to_send;
    } else {
      // 发送的数据少于第一个缓冲区的大小
      // 减少缓冲区大小,修改base
      m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
      m_iv[0].iov_base = m_write_buf + bytes_have_send;
    }
    if (bytes_to_send <= 0) {
      // 文件写入完毕
      // unmap
      unmap();
      // 将sockfd加入epoll实例
      modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
      if (m_linger) {
        // 复用连接
        init();
        return true;
      } else {
        return false;
      }
    }
  }
}

bool http_conn::add_response(const char *format, ...) {
  if (m_write_idx >= WRITE_BUFFER_SIZE) {
    return false;
  }
  va_list arg_list;
  va_start(arg_list, format);
  int len = vsnprintf(m_write_buf + m_write_idx,
                      WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);

  if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
    va_end(arg_list);
    return false;
  }
  m_write_idx += len;
  va_end(arg_list);
  LOG_INFO("request:%s", m_write_buf);
  return true;
}

bool http_conn::add_status_line(int status, const char *title) {
  return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len) {
  return add_content_length(content_len) && add_linger() && add_blank_line();
}
bool http_conn::add_content_length(int content_len) {
  return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::add_content_type() {
  return add_response("Content-Type:%s\r\n", "text/html");
}

bool http_conn::add_linger() {
  return add_response("Connection:%s\r\n",
                      (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line() { return add_response("%s", "\r\n"); }
bool http_conn::add_content(const char *content) {
  return add_response("%s", content);
}

bool http_conn::process_write(HTTP_CODE ret) {
  //  HTTP/1.1 200 OK
  switch (ret) {
  case INTERNAL_ERROR:
    add_status_line(500, error_500_title); // 500 INTERNAL_ERROR
    add_headers(strlen(error_500_form));
    if (!add_content(error_500_form)) {
      return false;
    }
    break;
  case BAD_REQUEST:
    add_status_line(404, error_404_title);
    add_headers(strlen(error_404_form));
    if (!add_content(error_404_form)) {
      return false;
    }
    break;
  case FORBIDDEN_REQUEST:
    add_status_line(403, error_403_title);
    add_headers(strlen(error_403_form));
    if (!add_content(error_403_form)) {
      return false;
    }
    break;
  case FILE_REQUEST:
    add_status_line(200, ok_200_title);
    if (m_file_stat.st_size != 0) {
      add_headers(m_file_stat.st_size);

      // 响应头和响应行
      m_iv[0].iov_base = m_write_buf;
      m_iv[0].iov_len = m_write_idx;

      // 文件信息
      m_iv[1].iov_base = m_file_address;
      m_iv[1].iov_len = m_file_stat.st_size;

      m_iv_count = 2;
      bytes_to_send = m_write_idx + m_file_stat.st_size;
      return true;

    } else {
      // 文件为空
      const char *ok_string = "<html><body></body></html>";
      add_headers(strlen(ok_string));
      if (!add_content(ok_string))
        return false;
    }
    break;
  default:
    return true;
  }

  m_iv[0].iov_base = m_write_buf;
  m_iv[0].iov_len = m_write_idx;
  m_iv_count = 1;
  bytes_to_send = m_write_idx;
  return true;
}

void http_conn::process() {
  HTTP_CODE read_ret = process_read();
  if (read_ret == NO_REQUEST) {
    // 继续监听读
    modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
    return;
  }
  // 读完 写入响应
  bool write_ret = process_write(read_ret);
  if (!write_ret) {
    close_conn();
  }
  // 接入写事件
  modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}