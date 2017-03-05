#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"
namespace mj{
	template< typename T >
	class threadpool
	{
	public:
		threadpool( int thread_num, int max_reqs);
		~threadpool();
		bool append( T* request );

	private:
		static void* worker( void* arg );
		void thread_run();

	private:
		int thread_number;
		int max_requests;
		pthread_t* all_threads;
		std::list< T* > business_queue;
		locker business_queue_locker;
		sem queue_sem;
		bool stop_all_threads;
	};

	template< typename T >
	threadpool< T >::threadpool( int thread_num, int max_req ) : 
		    thread_number( thread_num ), max_requests( max_req ), 
		    stop_all_threads( false ), all_threads( NULL )
	{
		if( ( thread_number <= 0 ) || ( max_requests <= 0 ) )
		{
		    throw std::exception();
		}

		all_threads = new pthread_t[ thread_number ];
		if( ! all_threads )
		{
		    throw std::exception();
		}

		for ( int i = 0; i < thread_number; ++i )
		{
		    printf( "create the %dth thread\n", i );
		    if( pthread_create( all_threads + i, NULL, worker, this ) != 0 )
		    {
		        delete [] all_threads;
		        throw std::exception();
		    }
		    if( pthread_detach( all_threads[i] ) )
		    {
		        delete [] all_threads;
		        throw std::exception();
		    }
		}
	}

	template< typename T >
	threadpool< T >::~threadpool()
	{
		delete [] all_threads;
		stop_all_threads = true;
	}

	template< typename T >
	bool threadpool< T >::append( T* request )
	{
		business_queue_locker.lock();
		if ( business_queue.size() > max_requests )
		{
		    business_queue_locker.unlock();
		    return false;
		}
		business_queue.push_back( request );
		business_queue_locker.unlock();
		queue_sem.post();
		return true;
	}

	template< typename T >
	void* threadpool< T >::worker( void* arg )
	{
		threadpool* pool = ( threadpool* )arg;
		pool->thread_run();
		return pool;
	}

	template< typename T >
	void threadpool< T >::thread_run()
	{
		while ( ! stop_all_threads )
		{
		    queue_sem.wait();
		    business_queue_locker.lock();
		    if ( business_queue.empty() )
		    {
		        business_queue_locker.unlock();
		        continue;
		    }
		    T* request = business_queue.front();
		    business_queue.pop_front();
		    business_queue_locker.unlock();
		    if ( ! request )
		    {
		        continue;
		    }
		    request->process();
		}
	}
}
#endif
