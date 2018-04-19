#include "httpd.h"

/********************************************************************/
/* 完成服务器端监听socket初始化、端口绑定，等待客户端连接
 * 参数：端口指针
 * 返回值：监听socket描述符  */
/********************************************************************/
int start_up(unsigned short *port)
{
	int httpd = 0;
	struct sockaddr_in name;

	// 创建soctet描述符
	httpd = socket(PF_INET, SOCK_STREAM, 0);
	if (httpd == -1)
	{
		error_die("socket");
	}

	memset(&name, 0, sizeof(name));
	name.sin_family = AF_INET;

	// htons: 主机字节序转网络字节序
	name.sin_port = htons(*port);

	// INADDR_ANY: IPV4的通配地址
	name.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind: 绑定地址与socket
	if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
	{
		error_die("bind");
	}

	// 调用bind后端口号仍是0, 手动调用getsockname() 获取端口号
	if (*port == 0)
	{
		socklen_t name_len = sizeof(name);
		if (getsockname(httpd, (struct sockaddr *)&name, &name_len) == -1)
		{
			error_die("getsockname");
		}
		*port = ntohs(name.sin_port);
	}

	// 5: 可以排队的最大连接个数
	if (listen(httpd, 5) < 0)
	{
		error_die("listen");
	}

	return httpd;
}


/********************************************************************/
/* 线程：处理客户端连接。读取http数据，针对不同类型请求，分别进行处理
 * 参数：连接socket描述符 */
/********************************************************************/
void accept_request(void* client_sock)
{
	char buf[1024];
	char method[255];
	char url[255];
	char path[512];
	int numchars;
	size_t i, j;
	struct stat st;
	int cgi = 0;	// becomes true if server decides this is a CGI program

	char *query_string = NULL;
	int client = *(int *)client_sock;

	
	// 读http请求的第一行数据到buf
	numchars = get_line(client, buf, sizeof(buf));

	// 将method存放在method数组中
	i = 0;
	j = 0;
	while (!IsSpace(buf[j]) && (i < sizeof(method) - 1) && j < sizeof(buf))
	{
		method[i] = buf[j];
		i++;
		j++;
	}
	method[i] = '\0';


	// 将url存放在url数组中
	i = 0;
	while (IsSpace(buf[j]) && (j < sizeof(buf)))
	{
		j++;
	}
	while (!IsSpace(buf[j]) && (i < sizeof(url) - 1) && j < sizeof(buf))
	{
		url[i] = buf[j];
		i++;
		j++;
	}
	url[i] = '\0';

	// 如果请求的方法不是 GET 或 POST 任意一个， 直接发送response 告诉客户端没有此方法
	if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
	{
		unimplemented(client);
		return;
	}


	// 如果请求的方法是POST方法，将cgi标志变量置true
	if (strcasecmp(method, "POST") == 0)
	{
		cgi = 1;
	}

	// 如果请求的方法是GET方法
	if (strcasecmp(method, "GET") == 0)
	{
		// 指针指向url
		query_string = url;

		// 遍历url，跳过'?'前面的所有字符
		while ((*query_string) != '?' && (*query_string) != '\0')
		{
			query_string++;
		}

		// url包含'?' 将cgi置为true
		if ((*query_string) == '?')
		{
			cgi = 1;

			// 将url 分为两部分
			query_string = '\0';
			query_string++;
		}
	}

	// 将url 放到path中
	sprintf(path, "htdocs%s", url);

	// 如果path数组中的字符串以 '/' 结尾，拼接 "index.html",表示首页
	if (path[strlen(path) - 1] == '/')
	{
		strcat(path, "index.html");
	}

	// 在系统上查询文件是否存在
	if (stat(path, &st) == -1)
	{
		// 不存在，把本次http请求的内容全部读完并忽略
		while ((numchars > 0) && strcmp("\n", buf))
		{
			numchars = get_line(client, buf, sizeof(buf));
		}

		// 返回找不到文件的 response
		not_found(client);
	}
	else
	{
		// 存在,判断文件类型
		if ((st.st_mode & S_IFMT) == S_IFDIR)
		{
			//文件是目录，在path后面拼接"/index.html"的字符串
			strcat(path, "index.html");
		}
		if ((st.st_mode & S_IXUSR) ||
			(st.st_mode & S_IXGRP) ||
			(st.st_mode & S_IXOTH))
		{
			// 文件是可执行文件
			cgi = 1;
		}


		if (!cgi)
		{
			// 不需要cgi机制
			serve_file(client, path);
		}
		else
		{
			// 需要cgi机制，调用
			execute_cgi(client, path, method, query_string);
		}
	}

	close(client);
}

/**********************************************************************/
/* 利用perror()进行错误信息打印
 * 参数：打印信息字符串 */
/**********************************************************************/
void error_die(const char *sc)
{
	perror(sc);
	exit(1);
}

/**********************************************************************/
/* 返回404 not found 错误 
 * 参数：socket描述符 */
/**********************************************************************/
void not_found(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "your request because the resource specified\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "is unavailable or nonexistent.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}

/*********************************************************************/
/* 发送文件到客户端
 * 参数: socket描述符
		文件路径 */
/*********************************************************************/
void serve_file(int client, const char *filename)
{
	FILE *resource = NULL;
	int numchars = 1;
	char buf[1024];

	// 确保buf里面有内容，可以进入while循环
	buf[0] = 'A';
	buf[1] = '\0';

	// 读取并忽略此http请求后面的所有内容
	while ((numchars > 0) && strcmp("\n", buf))
	{
		numchars = get_line(client, buf, sizeof(buf));
	}

	// 打开文件
	resource = fopen(filename, "r");
	if (resource == NULL)
	{
		not_found(client);
	}
	else
	{
		// 将文件的基本信息封装成 response 的头部返回
		headers(client, filename);
		
		// 文件内容作为 response 的 body 发送到客户端
		cat(client, resource);
	}
	fclose(resource);
}


/*********************************************************************/
/* 发送http头部信息. */
/* 参数：socket描述符
   		文件路径 */
/*********************************************************************/
void headers(int client, const char *filename)
{
	char buf[1024];
	(void)filename;

	strcpy(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, SERVER_STRING);
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
}


/*********************************************************************/
/* 发送文件内容信息 */
/* 参数：socket描述符
		发送的文件指针 */
/*********************************************************************/
void cat(int client, FILE *resource)
{
	char buf[2014];

	// 从文件描述符中读取指定内容
	fgets(buf, sizeof(buf), resource);
	while (!feof(resource))
	{
		send(client, buf, strlen(buf), 0);
		fgets(buf, sizeof(buf), resource);
	}
}

/*********************************************************************/
/* 按行读取socket数据
 * 参数：socket描述符
    	存储接收数据的字符数组
 		数组空间长度 */
/*********************************************************************/
int get_line(int sock, char *buf, int size)
{
	int i = 0;
	char c = '\0';
	int n = 0;

	while ((i < size -1) && (c != '\n'))
	{
		// 读取一个字节的数存放在 c 中
		n = recv(sock, &c, 1, 0);
		if (n > 0)
		{
			if (c == '\r')
			{
				n = recv(sock, &c, 1, MSG_PEEK);
				if ((n > 0) && (c == '\n'))
				{
					recv(sock, &c, 1, 0);
				}
				else
				{
					c = '\n';
				}
			}
			buf[i] = c;
			i++;
		}
		else
		{
			c = '\n';
		}
		buf[i] = '\0';
	}
	return i;
}

/*****************************************************************/
/* 创建子进程，执行 CGI 脚本
 * 参数：socket描述符
 		CGI 脚本路径
 		method： POST/GET
 		query字符串*/
/*****************************************************************/
void execute_cgi(int client, const char *path, const char *method, const char *query_string)
{
	char buf[1024];
	int cgi_output[2];
	int cgi_input[2];
	pid_t pid;
	int status;
	int i;
	char c;
	int numchars = 1;
	int content_length = -1;

	// 确保buf里面有内容，可以进入while循环
	buf[0] = 'A';
	buf[1] = '\0';
	
	// 如果http请求是get方法，读取并忽略请求剩下的内容
	if ((strcasecmp(method, "GET")) == 0)
	{
		while ((numchars > 0) && strcmp("\n", buf))
		{
			numchars = get_line(client, buf, sizeof(buf));
		}
	}
	else	// POST方法
	{
		// 继续读取内容
		numchars = get_line(client, buf, sizeof(buf));
		// 读出指示body长度大小的参数，其余header里面的参数忽略
		while ((numchars > 0) && strcmp("\n", buf))
		{
			buf[15] = '\0';
			if (strcasecmp(buf, "Content-Length:") == 0)
			{
				// body 长度
				content_length = atoi(&buf[16]);
			}
			numchars = get_line(client, buf, sizeof(buf));
		}

		// http 请求的 header 没有指示body长度大小的参数
		if (content_length == -1)
		{
			bad_request(client);
			return;
		}
	}

	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	send(client, buf, strlen(buf), 0);

	// 创建两个管道，用于进程间通信
	if (pipe(cgi_output) < 0)
	{
		cannot_execute(client);
		return;
	}
	if (pipe(cgi_input) < 0)
	{
		cannot_execute(client);
		return;
	}


	// 创建子进程，用来执行cgi脚本
	if ((pid = fork()) < 0)
	{
		cannot_execute(client);
		return;
	}

	// 子进程
	if (pid == 0)
	{
		char meth_env[255];
		char query_env[255];
		char length_env[255];

		// 将子进程的输出由标准输出重定向到 cgi_output 的管道写端
		dup2(cgi_output[1], 1);

		// 将子进程的输入由标准输入重定向到 cgi_input 的管道读端
		dup2(cgi_input[0], 0);

		// 关闭 cgi_output 的读端与 cgi_input 的写端
		close(cgi_output[0]);
		close(cgi_input[1]);

		// 构造环境变量
		sprintf(meth_env, "REQUEST_METHOD=%s", method);
		// 将环境变量加入子进程的运行环境
		putenv(meth_env);

		// 根据 http 请求的不同方法，构造并存储不同的环境变量
		if (strcasecmp(method, "GET") == 0)
		{
			sprintf(query_env, "QUERY_ETRING=%s", query_string);
			putenv(query_env);
		}
		else
		{
			sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
			putenv(length_env);
		}

		// 将子进程替换为另一进程并执行cgi脚本
		execl(path, path, NULL);
		exit(0);
	}
	else	// 父进程
	{
		// 关闭 cgi_output 管道的写端和 cgi_input管道的读端
		close(cgi_output[1]);
		close(cgi_input[0]);

		// 如果是POST方法，继续读body内容，写到cgi_input管道，传给子进程
		if (strcasecmp(method, "POST") == 0)
		{
			for (i = 0; i < content_length; i++)
			{
				recv(client, &c, 1, 0);
				write(cgi_input[1], &c, 1);
			}
		}

		// 从cgi_output管道中读取子进程输出，发送到客户端
		while (read(cgi_output[0], &c, 1) > 0)
		{
			send(client, &c, 1, 0);
		}

		// 关闭管道
		close(cgi_output[0]);
		close(cgi_input[1]);

		// 等待子进程退出
		waitpid(pid, &status, 0);
	}
}

/****************************************************************/
/* 通知client CGI脚本执行错误
 * 参数：socket描述符 */
/****************************************************************/
void cannot_execute(int client)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "/r/n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
	send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* 通知客户端 request 错误
 * 参数：socket描述符 */
/**********************************************************************/
void bad_request(int client)
{
	 char buf[1024];

	sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "Content-type: text/html\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "\r\n");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "<P>Your browser sent a bad request, ");
	send(client, buf, sizeof(buf), 0);
	sprintf(buf, "such as a POST without a Content-Length.\r\n");
	send(client, buf, sizeof(buf), 0);
}


/**********************************************************************/
/* 通知客户端方法未实现
 * 参数：socket描述符 */
/**********************************************************************/
void unimplemented(int client)
{
	char buf[1024];

 	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
 	send(client, buf, strlen(buf), 0);
 	sprintf(buf, SERVER_STRING);
 	send(client, buf, strlen(buf), 0);
 	sprintf(buf, "Content-Type: text/html\r\n");
 	send(client, buf, strlen(buf), 0);
 	sprintf(buf, "\r\n");
 	send(client, buf, strlen(buf), 0);
 	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
 	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</TITLE></HEAD>\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "</BODY></HTML>\r\n");
	send(client, buf, strlen(buf), 0);
}


int main()
{
	int server_sock = -1;
	int client_sock = -1;
	unsigned short port = 4000;	//port=0,随机分配监听端口，否则在指定端口进行监听
	pthread_t newthread;

	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);

	server_sock = start_up(&port);
	printf("httpd running on port %d\n", port);
	while(1)
	{
		//阻塞等待客户端连接
		client_sock = accept(server_sock, (struct sockaddr *)(&client_name), &client_name_len);
		if (client_sock == -1)
		{
			error_die("accept");
		}
		int client_sock_thread = client_sock;
		if (pthread_create(&newthread, NULL, (void *)accept_request, (void*)&client_sock_thread) != 0)
		{
			perror("thread_create");
		}
	}

	close(server_sock);

	return 0;
}
