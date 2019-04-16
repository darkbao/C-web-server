#ifndef __MJ_THREADPOOL_H__
#define __MJ_THREADPOOL_H__

#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <exception>
#include "public_func.h"

namespace mj
{

static const int MAX_EVENT_NUMBER = 30000;
static const int MAX_FD = 65535;

template<typename T>
class threadpool
{
public:
	threadpool(unsigned thread_num, int port) : m_thread_num(thread_num), m_threads(NULL), m_port(port)
	{
		if ((thread_num <= 0) || (port <= 0)) {
		    throw std::exception();
		}

		m_threads = new pthread_t[m_thread_num];
		if (!m_threads) {
		    throw std::exception();
		}

		for (unsigned i = 0; i < m_thread_num; ++i) {
		    if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
		        delete[] m_threads;
		        throw std::exception();
		    }
		}
		printf("total create [%u] workers thread, listen port[%d]\n", thread_num, port);
	}

	~threadpool()
	{
		if (m_threads) {
			delete[] m_threads;
		}
	}

	inline const pthread_t* getAllThreadID() const
	{
		return m_threads;
	}

private:
	static void* worker(void* arg)
	{
		threadpool* pool = (threadpool*)arg;
		pool->thread_run();
		return pool;
	}

	void thread_run()
	{
	    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	    assert(listenfd >= 0);
	   
	    int opt_on = 1;
	    setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &opt_on, sizeof(opt_on));
	    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt_on, sizeof(opt_on));

	   	struct sockaddr_in address;
	    bzero(&address, sizeof(address));
		address.sin_family      = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_ANY);
		address.sin_port        = htons(m_port);

	    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
	    assert(ret == 0);
	    ret = listen(listenfd, 5);
	    assert(ret == 0);
	    setNonBlock(listenfd);

	    int epollfd = epoll_create(5);
	    assert(epollfd >= 0);
	    updateEvents(epollfd, listenfd, EPOLLIN, EPOLL_CTL_ADD);

	    eventLoop(epollfd, listenfd, 10000);
	    removeAndClose(epollfd, listenfd);
	    close(epollfd);
	}

	void eventLoop(int epollfd, int listenfd, int waitMs)
	{
		T* user = new T[MAX_FD];
		epoll_event activeEvent[MAX_EVENT_NUMBER];
	    printf("eventLoop begin, epollfd[%d], listenfd[%d], threadID[%lu]\n", epollfd, listenfd, pthread_self());
	    while (true) {
	        int number = epoll_wait(epollfd, activeEvent, MAX_EVENT_NUMBER, waitMs);
	        for (int i = 0; i < number; i++) {
	            int sockfd = activeEvent[i].data.fd;
        		int events = activeEvent[i].events;
	            if (sockfd == listenfd) {
	                struct sockaddr_in client_address;
	                socklen_t client_addrlength = sizeof(client_address);
	                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
	                if (connfd < 0) {
	                    printf("[error] accept failed, errno is: %d\n", errno);
	                    continue;
	                }
	                user[connfd].init(connfd, epollfd, client_address);
					setNonBlock(connfd);
    				updateEvents(epollfd, connfd, EPOLLIN | EPOLLOUT | EPOLLET, EPOLL_CTL_ADD);
	                printf("accept sockfd[%d]\n", connfd);
	            } else if(events & EPOLLIN) {
	                user[sockfd].read();
	                user[sockfd].process();
	            } else if(events & EPOLLOUT) {
	                user[sockfd].write();
	            } else {
					printf("[error] unknown event type\n");
					exit(-1);
	            }
	        }
	    }
	    delete[] user;
	}
private:
	unsigned int		m_thread_num;
	pthread_t* 			m_threads;
	int 			    m_port;
};
	
}
#endif
