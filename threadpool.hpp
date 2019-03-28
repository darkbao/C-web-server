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
	threadpool(unsigned thread_num, int port)
		   	 : m_thread_num(thread_num),
	 		   m_threads(NULL),
	 		   m_port(port)
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
	    
	    struct linger tmp = { 1, 0 };
	    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
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

	    int epollfd = epoll_create(5);
	    assert(epollfd != -1);
	    addfd(epollfd, listenfd);

		T* user_vec = new T[MAX_FD];
		epoll_event event_vec[MAX_EVENT_NUMBER];
		unsigned int user_num = 0;
	    printf("thread[%lu] loop begin, epollfd[%d], listenfd[%d]\n", pthread_self(), epollfd, listenfd);

	    while (true) {
	        int number = epoll_wait(epollfd, event_vec, MAX_EVENT_NUMBER, -1);
	        if (number <= 0) {
	            printf("[error] epoll_wait failure, errno[%d]\n", errno);
	            if (errno == EINTR) {
	            	continue;
	            } else {
	            	exit(-1);
	            }
	        } else {
	        	printf("thread[%lu], epoll_wait return, number[%d]\n", pthread_self(), number);
	        }

	        for (int i = 0; i < number; i++) {
	            int sockfd = event_vec[i].data.fd;
	            if (sockfd == listenfd) {
	                struct sockaddr_in client_address;
	                socklen_t client_addrlength = sizeof(client_address);
	                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
	                if (connfd < 0) {
	                    printf("[error] accept failed, errno is: %d\n", errno);
	                    continue;
	                }
	                if (++user_num >= MAX_FD) {
	                    send_error(connfd, "internal server busy\n");
	                    continue;
	                }
	                user_vec[connfd].init(connfd, client_address);
					addfd(epollfd, connfd);
	                printf("thread[%lu] accept sockfd[%d]\n", pthread_self(), connfd);
	            } else if(event_vec[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
	                removefd(epollfd, sockfd);
	                --user_num;
	                printf("peer close sockfd[%d]\n", sockfd);
	            } else if(event_vec[i].events & EPOLLIN) {
	                if (user_vec[sockfd].read()) {
	                   	int ret = user_vec[sockfd].process();
	                   	if (ret == 1) {
	                    	printf("incomplete_request, sockfd[%d]\n", sockfd);
	                   	} else if (ret == -1) {
	                   		printf("sockfd[%d] process write failed, close it\n", sockfd);
	                   		removefd(epollfd, sockfd);
	                		--user_num;
	                   	} else {
	                   		printf("sockfd[%d] read and process succeed\n", sockfd);
							modfd(epollfd, sockfd, EPOLLOUT);
	                   	}
	                } else {
	                	removefd(epollfd, sockfd);
	                	--user_num;
	                    printf("[error] sockfd[%d] read failed, close it\n", sockfd);
	                }
	            } else if(event_vec[i].events & EPOLLOUT) {
	                if (user_vec[sockfd].write() == false) {
	                	removefd(epollfd, sockfd);
	                	--user_num;
	                    printf("[error] sockfd[%d] write failed, close it\n", sockfd);
	                }
	                if (user_vec[sockfd].isKeepAlive()) {
	                	user_vec[sockfd].reset();
						modfd(epollfd, sockfd, EPOLLIN);
	                	printf("write done, keep-alive, sockfd[%d]\n", sockfd);
	                } else {
	                	removefd(epollfd, sockfd, true);
	                	--user_num;
	                	printf("write done, close sockfd[%d]\n", sockfd);
	                }
	            } else {
					printf("[error] unknown event type\n");
	            }
	        }
	    }
	    close(listenfd);
	    close(epollfd);
	    delete[] user_vec;
	}

private:
	unsigned int		m_thread_num;
	pthread_t* 			m_threads;
	int 			    m_port;
};
	
}
#endif
