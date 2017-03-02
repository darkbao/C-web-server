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
		threadpool( int thread_number = 8, int max_requests = 10000 );
		~threadpool();
		bool append( T* request );

	private:
		static void* worker( void* arg );
		void run();

	private:
		int mj_thread_number;
		int mj_max_requests;
		pthread_t* mj_threads;
		std::list< T* > mj_workqueue;
		locker mj_queuelocker;
		sem mj_queuestat;
		bool mj_stop;
	};

	template< typename T >
	threadpool< T >::threadpool( int thread_number, int max_requests ) : 
		    mj_thread_number( thread_number ), mj_max_requests( max_requests ), mj_stop( false ), mj_threads( NULL )
	{
		if( ( thread_number <= 0 ) || ( max_requests <= 0 ) )
		{
		    throw std::exception();
		}

		mj_threads = new pthread_t[ mj_thread_number ];
		if( ! mj_threads )
		{
		    throw std::exception();
		}

		for ( int i = 0; i < thread_number; ++i )
		{
		    printf( "create the %dth thread\n", i );
		    if( pthread_create( mj_threads + i, NULL, worker, this ) != 0 )
		    {
		        delete [] mj_threads;
		        throw std::exception();
		    }
		    if( pthread_detach( mj_threads[i] ) )
		    {
		        delete [] mj_threads;
		        throw std::exception();
		    }
		}
	}

	template< typename T >
	threadpool< T >::~threadpool()
	{
		delete [] mj_threads;
		mj_stop = true;
	}

	template< typename T >
	bool threadpool< T >::append( T* request )
	{
		mj_queuelocker.lock();
		if ( mj_workqueue.size() > mj_max_requests )
		{
		    mj_queuelocker.unlock();
		    return false;
		}
		mj_workqueue.push_back( request );
		mj_queuelocker.unlock();
		mj_queuestat.post();
		return true;
	}

	template< typename T >
	void* threadpool< T >::worker( void* arg )
	{
		threadpool* pool = ( threadpool* )arg;
		pool->run();
		return pool;
	}

	template< typename T >
	void threadpool< T >::run()
	{
		while ( ! mj_stop )
		{
		    mj_queuestat.wait();
		    mj_queuelocker.lock();
		    if ( mj_workqueue.empty() )
		    {
		        mj_queuelocker.unlock();
		        continue;
		    }
		    T* request = mj_workqueue.front();
		    mj_workqueue.pop_front();
		    mj_queuelocker.unlock();
		    if ( ! request )
		    {
		        continue;
		    }
		    request->process();
		}
	}
}
#endif
