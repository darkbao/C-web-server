/*
	http_business.cpp
	process http request and send back a .html file
*/

#include "http_business.h"

int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

void modfd(int epollfd, int fd, int ev)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void addfd(int epollfd, int fd, bool one_shot)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	if (one_shot)
	{
		event.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

void removefd(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

namespace mj{
	const char* ok_200_title = "OK";
	const char* error_400_title = "Bad Request";
	const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
	const char* error_403_title = "Forbidden";
	const char* error_403_form = "You do not have permission to get file from this server.\n";
	const char* error_404_title = "Not Found";
	const char* error_404_form = "The requested file was not found on this server.\n";
	const char* error_500_title = "Internal Error";
	const char* error_500_form = "There was an unusual problem serving the requested file.\n";
	const char* doc_root = "/var/www/html";

	int http_business::mj_user_count = 0;
	int http_business::mj_epollfd = -1;

	void http_business::close_conn(bool real_close)
	{
		if (real_close && (mj_sockfd != -1))
		{
			fprintf(stderr,"client: %d close!\n",mj_sockfd);
			removefd(mj_epollfd, mj_sockfd);
			mj_sockfd = -1;
			mj_user_count--;
		}
	}

	void http_business::init(int sockfd, const sockaddr_in& addr)
	{
		fprintf(stderr,"client: %d join!\n",sockfd);
		mj_sockfd = sockfd;
		mj_address = addr;
		int error = 0;
		socklen_t len = sizeof(error);
		getsockopt(mj_sockfd, SOL_SOCKET, SO_ERROR, &error, &len);
		int reuse = 1;
		setsockopt(mj_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
		addfd(mj_epollfd, sockfd, true);
		mj_user_count++;

		init();
	}

	void http_business::init()
	{
		mj_check_state = CHECK_STATE_REQUESTLINE;
		mj_linger = false;

		mj_method = GET;
		mj_url = 0;
		mj_version = 0;
		mj_content_length = 0;
		mj_host = 0;
		mj_start_line = 0;
		mj_checked_idx = 0;
		mj_read_idx = 0;
		mj_write_idx = 0;
		memset(mj_read_buf, '\0', READ_BUFFER_SIZE);
		memset(mj_write_buf, '\0', WRITE_BUFFER_SIZE);
		memset(mj_real_file, '\0', FILENAME_LEN);
	}

	http_business::LINE_STATUS http_business::parse_line()
	{
		char temp;
		for (; mj_checked_idx < mj_read_idx; ++mj_checked_idx)
		{
			temp = mj_read_buf[mj_checked_idx];
			if (temp == '\r')
			{
				if ((mj_checked_idx + 1) == mj_read_idx)
				{
					return LINE_OPEN;
				}
				else if (mj_read_buf[mj_checked_idx + 1] == '\n')
				{
					mj_read_buf[mj_checked_idx++] = '\0';
					mj_read_buf[mj_checked_idx++] = '\0';
					return LINE_OK;
				}

				return LINE_BAD;
			}
			else if (temp == '\n')
			{
				if ((mj_checked_idx > 1) && (mj_read_buf[mj_checked_idx - 1] == '\r'))
				{
					mj_read_buf[mj_checked_idx - 1] = '\0';
					mj_read_buf[mj_checked_idx++] = '\0';
					return LINE_OK;
				}
				return LINE_BAD;
			}
		}

		return LINE_OPEN;
	}

	bool http_business::read()
	{
		if (mj_read_idx >= READ_BUFFER_SIZE)
		{
			return false;
		}

		int bytes_read = 0;
		while (true)
		{
			bytes_read = recv(mj_sockfd, mj_read_buf + mj_read_idx, READ_BUFFER_SIZE - mj_read_idx, 0);
			if (bytes_read == -1)
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK)
				{
					break;
				}
				return false;
			}
			else if (bytes_read == 0)
			{
				return false;
			}

			mj_read_idx += bytes_read;
		}
		return true;
	}

	http_business::HTTP_CODE http_business::parse_request_line(char* text)
	{
		mj_url = strpbrk(text, " \t");
		if (!mj_url)
		{
			return BAD_REQUEST;
		}
		*mj_url++ = '\0';

		char* method = text;
		/*only parse method "GET"*/
		if (strcasecmp(method, "GET") == 0)
		{
			mj_method = GET;
		}
		else
		{
			return BAD_REQUEST;
		}

		mj_url += strspn(mj_url, " \t");
		mj_version = strpbrk(mj_url, " \t");
		if (!mj_version)
		{
			return BAD_REQUEST;
		}
		*mj_version++ = '\0';
		mj_version += strspn(mj_version, " \t");
		if (strcasecmp(mj_version, "HTTP/1.1") != 0)
		{
			return BAD_REQUEST;
		}

		if (strncasecmp(mj_url, "http://", 7) == 0)
		{
			mj_url += 7;
			mj_url = strchr(mj_url, '/');
		}

		if (!mj_url || mj_url[0] != '/')
		{
			return BAD_REQUEST;
		}

		mj_check_state = CHECK_STATE_HEADER;
		return INCOMPLETE_REQUEST;
	}

	http_business::HTTP_CODE http_business::parse_headers(char* text)
	{
		if (text[0] == '\0')
		{
			if (mj_method == HEAD)
			{
				return GET_REQUEST;
			}

			if (mj_content_length != 0)
			{
				mj_check_state = CHECK_STATE_CONTENT;
				return INCOMPLETE_REQUEST;
			}

			return GET_REQUEST;
		}
		else if (strncasecmp(text, "Connection:", 11) == 0)
		{
			text += 11;
			text += strspn(text, " \t");
			if (strcasecmp(text, "keep-alive") == 0)
			{
				/*record if client wants to keep this connection*/
				mj_linger = true;
			}
		}
		else if (strncasecmp(text, "Content-Length:", 15) == 0)
		{
			text += 15;
			text += strspn(text, " \t");
			mj_content_length = atol(text);
		}
		else if (strncasecmp(text, "Host:", 5) == 0)
		{
			text += 5;
			text += strspn(text, " \t");
			mj_host = text;
		}
		else
		{
			//fprintf(stderr,"oop! unknow header %s\n", text);
		}

		return INCOMPLETE_REQUEST;

	}

	http_business::HTTP_CODE http_business::parse_content(char* text)
	{
		if (mj_read_idx >= (mj_content_length + mj_checked_idx))
		{
			text[mj_content_length] = '\0';
			return GET_REQUEST;
		}

		return INCOMPLETE_REQUEST;
	}

	http_business::HTTP_CODE http_business::process_read()
	{
		LINE_STATUS line_status = LINE_OK;
		HTTP_CODE ret = INCOMPLETE_REQUEST;
		char* text = 0;

		while (((mj_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
			|| ((line_status = parse_line()) == LINE_OK))
		{
			text = get_line();
			mj_start_line = mj_checked_idx;
			// fprintf(stderr,  "got 1 http line: %s\n", text );

			switch (mj_check_state)
			{
			case CHECK_STATE_REQUESTLINE:
			{
											ret = parse_request_line(text);
											if (ret == BAD_REQUEST)
											{
												return BAD_REQUEST;
											}
											break;
			}
			case CHECK_STATE_HEADER:
			{
									   ret = parse_headers(text);
									   if (ret == BAD_REQUEST)
									   {
										   return BAD_REQUEST;
									   }
									   else if (ret == GET_REQUEST)
									   {
										   return do_request();
									   }
									   break;
			}
			case CHECK_STATE_CONTENT:
			{
										ret = parse_content(text);
										if (ret == GET_REQUEST)
										{
											return do_request();
										}
										line_status = LINE_OPEN;
										break;
			}
			default:
			{
					   return INTERNAL_ERROR;
			}
			}
		}

		return INCOMPLETE_REQUEST;
	}

	http_business::HTTP_CODE http_business::do_request()
	{
		strcpy(mj_real_file, doc_root);
		int len = strlen(doc_root);
		strncpy(mj_real_file + len, mj_url, FILENAME_LEN - len - 1);
		if (stat(mj_real_file, &mj_file_stat) < 0)
		{
			return NO_RESOURCE;
		}

		if (!(mj_file_stat.st_mode & S_IROTH))
		{
			return FORBIDDEN_REQUEST;
		}

		if (S_ISDIR(mj_file_stat.st_mode))
		{
			return BAD_REQUEST;
		}

		int fd = open(mj_real_file, O_RDONLY);
		mj_file_address = (char*)mmap(0, mj_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		close(fd);
		return FILE_REQUEST;
	}

	void http_business::unmap()
	{
		if (mj_file_address)
		{
			munmap(mj_file_address, mj_file_stat.st_size);
			mj_file_address = 0;
		}
	}

	bool http_business::write()
	{
		int temp = 0;
		int bytes_have_send = 0;
		int bytes_to_send = mj_write_idx;
		if (bytes_to_send == 0)
		{
			modfd(mj_epollfd, mj_sockfd, EPOLLIN);
			init();
			return true;
		}

		while (1)
		{
			temp = writev(mj_sockfd, mj_iv, mj_iv_count);
			if (temp <= -1)
			{
				if (errno == EAGAIN)
				{
					modfd(mj_epollfd, mj_sockfd, EPOLLOUT);
					return true;
				}
				unmap();
				return false;
			}

			bytes_to_send -= temp;
			bytes_have_send += temp;
			if (bytes_to_send <= bytes_have_send)
			{
				unmap();
				if (mj_linger)
				{
					init();//
					modfd(mj_epollfd, mj_sockfd, EPOLLIN);
					return true;
				}
				else
				{
					modfd(mj_epollfd, mj_sockfd, EPOLLIN);
					return false;
				}
			}
		}
	}

	bool http_business::add_response(const char* format, ...)
	{
		if (mj_write_idx >= WRITE_BUFFER_SIZE)
		{
			return false;
		}
		va_list arg_list;
		va_start(arg_list, format);
		int len = vsnprintf(mj_write_buf + mj_write_idx, WRITE_BUFFER_SIZE - 1 - mj_write_idx, format, arg_list);
		if (len >= (WRITE_BUFFER_SIZE - 1 - mj_write_idx))
		{
			return false;
		}
		mj_write_idx += len;
		va_end(arg_list);
		return true;
	}

	bool http_business::add_status_line(int status, const char* title)
	{
		return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
	}

	bool http_business::add_headers(int content_len)
	{
		add_content_length(content_len);
		add_linger();
		add_blank_line();
	}

	bool http_business::add_content_length(int content_len)
	{
		return add_response("Content-Length: %d\r\n", content_len);
	}

	bool http_business::add_linger()
	{
		return add_response("Connection: %s\r\n", (mj_linger == true) ? "keep-alive" : "close");
	}

	bool http_business::add_blank_line()
	{
		return add_response("%s", "\r\n");
	}

	bool http_business::add_content(const char* content)
	{
		return add_response("%s", content);
	}

	bool http_business::process_write(HTTP_CODE ret)
	{
		switch (ret)
		{
		case INTERNAL_ERROR:
		{
							   add_status_line(500, error_500_title);
							   add_headers(strlen(error_500_form));
							   if (!add_content(error_500_form))
							   {
								   return false;
							   }
							   break;
		}
		case BAD_REQUEST:
		{
							add_status_line(400, error_400_title);
							add_headers(strlen(error_400_form));
							if (!add_content(error_400_form))
							{
								return false;
							}
							break;
		}
		case NO_RESOURCE:
		{
							add_status_line(404, error_404_title);
							add_headers(strlen(error_404_form));
							if (!add_content(error_404_form))
							{
								return false;
							}
							break;
		}
		case FORBIDDEN_REQUEST:
		{
								  add_status_line(403, error_403_title);
								  add_headers(strlen(error_403_form));
								  if (!add_content(error_403_form))
								  {
									  return false;
								  }
								  break;
		}
		case FILE_REQUEST:
		{
							 add_status_line(200, ok_200_title);
							 if (mj_file_stat.st_size != 0)
							 {
								 add_headers(mj_file_stat.st_size);
								 mj_iv[0].iov_base = mj_write_buf;
								 mj_iv[0].iov_len = mj_write_idx;
								 mj_iv[1].iov_base = mj_file_address;
								 mj_iv[1].iov_len = mj_file_stat.st_size;
								 mj_iv_count = 2;
								 return true;
							 }
							 else
							 {
								 const char* ok_string = "<html><body></body></html>";
								 add_headers(strlen(ok_string));
								 if (!add_content(ok_string))
								 {
									 return false;
								 }
							 }
		}
		default:
		{
				   return false;
		}
		}

		mj_iv[0].iov_base = mj_write_buf;
		mj_iv[0].iov_len = mj_write_idx;
		mj_iv_count = 1;
		return true;
	}

	void http_business::process()
	{
		HTTP_CODE read_ret = process_read();
		if (read_ret == INCOMPLETE_REQUEST)
		{
			modfd(mj_epollfd, mj_sockfd, EPOLLIN);
			return;
		}

		bool write_ret = process_write(read_ret);
		if (!write_ret)
		{
			close_conn();
		}

		modfd(mj_epollfd, mj_sockfd, EPOLLOUT);
		fprintf(stderr, "processing done!");
	}
}
