#ifndef __MJ_THREADPOOL_H__
#define __MJ_THREADPOOL_H__
#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

namespace mj
{

template<typename T>
class threadpool
{
public:
	threadpool(unsigned thread_num, unsigned max_reqs) : thread_number(thread_num)
			   ,max_requests(max_reqs), stop_all_threads(false), all_threads(NULL)
	{
		if ((thread_number <= 0) || (max_requests <= 0)) {
		    throw std::exception();
		}
		all_threads = new pthread_t[thread_number];
		if (!all_threads) {
		    throw std::exception();
		}

		for (unsigned i = 0; i < thread_number; ++i) {
		    if (pthread_create(all_threads + i, NULL, worker, this) != 0) {
		        delete [] all_threads;
		        throw std::exception();
		    }
		    if (pthread_detach(all_threads[i])) {
		        delete [] all_threads;
		        throw std::exception();
		    }
		}
		printf("total [%u] worker thread created\n", thread_number);
	}

	~threadpool()
	{
		delete [] all_threads;
		stop_all_threads = true;
	}

	bool append(T* request)
	{
		business_queue_locker.lock();
		if (business_queue.size() > max_requests) {
		    business_queue_locker.unlock();
		    return false;
		}
		business_queue.push_back(request);
		business_queue_locker.unlock();
		queue_sem.post();
		return true;
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
		while (!stop_all_threads) {
		    queue_sem.wait();
		    business_queue_locker.lock();
		    if (business_queue.empty()) {
		        business_queue_locker.unlock();
		        continue;
		    }
		    T* request = business_queue.front();
		    business_queue.pop_front();
		    business_queue_locker.unlock();
		    if (!request) {
		        continue;
		    }
		    request->process();
		}
	}

private:
	unsigned 		thread_number;
	unsigned 		max_requests;
	bool     		stop_all_threads;
	pthread_t* 		all_threads;
	std::list<T*> 	business_queue;
	locker 			business_queue_locker;
	sem 		   	queue_sem;
};
	
}
#endif
