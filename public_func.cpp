#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <signal.h>
#include "public_func.h"

namespace mj
{

void setNonBlock(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
    exit_if(old_option < 0, "fcntl failed");
	int ret = fcntl(fd, F_SETFL, old_option | O_NONBLOCK);
    exit_if(ret < 0, "fcntl failed");
}


void updateEvents(int epollfd, int fd, int ev, int op)
{
	epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = ev;
    event.data.fd = fd;
    int r = epoll_ctl(epollfd, op, fd, &event);
    exit_if(r, "epoll_ctl failed");
}

void removeAndClose(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

void addsig(int sig, void(handler)(int), bool restart)
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

}