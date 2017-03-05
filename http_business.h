#ifndef HTTP_BUSINESS_H
#define HTTP_BUSINESS_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>

namespace mj{
	class http_business
	{
	public:
		static const int FILENAME_LEN = 200;
		static const int READ_BUFFER_SIZE = 2048;
		static const int WRITE_BUFFER_SIZE = 1024;
		enum METHOD { GET, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH };
		enum CHECK_STATE { CHECK_STATE_REQUESTLINE, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
		enum HTTP_CODE { INCOMPLETE_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, 
			              FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
		enum LINE_STATUS { LINE_OK, LINE_BAD, LINE_OPEN };

	public:
		http_business(){}
		~http_business(){}

	public:
		void init(int sockfd, const sockaddr_in& addr);
		void close_conn(bool real_close = true);
		void process();
		bool read();
		bool write();

	private:
		void init();
		HTTP_CODE process_read();
		bool process_write(HTTP_CODE ret);

		HTTP_CODE parse_request_line(char* text);
		HTTP_CODE parse_headers(char* text);
		HTTP_CODE parse_content(char* text);
		HTTP_CODE do_request();
		char* get_line() { return http_read_buf + http_start_line; }
		LINE_STATUS parse_line();

		void unmap();
		bool add_response(const char* format, ...);
		bool add_content(const char* content);
		bool add_status_line(int status, const char* title);
		bool add_headers(int content_length);
		bool add_content_length(int content_length);
		bool add_linger();
		bool add_blank_line();

	public:
		static int http_epollfd;
		static int http_user_count;

	private:
		int http_sockfd;
		sockaddr_in http_address;

		char http_read_buf[READ_BUFFER_SIZE];
		int http_read_idx;
		int http_checked_idx;
		int http_start_line;
		char http_write_buf[WRITE_BUFFER_SIZE];
		int http_write_idx;

		CHECK_STATE http_check_state;
		METHOD http_method;

		char http_real_file[FILENAME_LEN];
		char* http_url;
		char* http_version;
		char* http_host;
		int http_content_length;
		bool http_keep_alive;

		char* http_file_address;
		struct stat http_file_stat;//描述文件属性
		struct iovec http_iv[2];
		//I/O vector，与readv和wirtev操作相关的结构体。
		//readv和writev函数用于在一次函数调用中读、写多个非连续缓冲区。
		//有时也将这两个函数称为散布读（scatter read）和聚集写（gather write）
		int http_iv_count;
	};
}
#endif
