#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <signal.h>

namespace mj
{

int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}


void addfd(int epollfd, int fd)
{
	epoll_event event;
	event.data.fd = fd;
	event.events  = EPOLLIN | EPOLLET | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

void removefd(int epollfd, int fd, bool half_close)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	if (half_close) {
		shutdown(fd, SHUT_RDWR);
	} else {
		close(fd);
	}
}

void modfd(int epollfd, int fd, int ev)
{
	epoll_event event;
	event.data.fd = fd;
	event.events  = ev | EPOLLET | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void addsig(int sig, void(handler)(int), bool restart = true)
{
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	if (restart) {
		sa.sa_flags |= SA_RESTART;
	}
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);
}

void send_error(int connfd, const char* info)
{
	printf("%s", info);
	send(connfd, info, strlen(info), 0);
	close(connfd);
}

}