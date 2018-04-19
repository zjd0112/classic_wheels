#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>

// 定义server名称
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

// 检查 x 是否为空白符
#define IsSpace(x) isspace((int)(x))

// 处理监听到的HTTP请求
void accept_request(void *);

// 返回客户端错误请求
void bad_request(int);

// 读取服务器文件，写到socket套接字
void cat(int, FILE *);

// 处理执行 cgi 期间发生的错误
void cannot_execute(int);

// 错误信息写道perror
void error_die(const char *); 

// 创建子进程，运行 cgi脚本
void execute_cgi(int, const char *, const char *, const char *); 

// 读取一行 HTTP 报文
int get_line(int, char *, int);

// 返回 HTTP 头部 
void headers(int, const char *);

// 找不到文件 response 
void not_found(int);

// 调用 cat 将文件内容返回浏览器
void serve_file(int, const char *);

// 开启 HTTP 服务
int start_up(u_short *);

// 返回浏览器，HTTP请求的method不被支持
void unimplemented(int);

