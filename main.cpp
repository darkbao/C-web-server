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
#include "public_func.h"
#include "locker.h"
#include "threadpool.h"
#include "http_business.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 30000
#define POOL_THREAD_NUM 20

using namespace mj;

int main( int argc, char* argv[] )
{
    if( argc <= 1 )
    {
        printf( "usage: %s port_number\n", basename( argv[0] ) );
        return 1;
    }
    int port = atoi( argv[1] );

    addsig( SIGPIPE, SIG_IGN );

    threadpool< http_business >* pool = NULL;
    try
    {
        pool = new threadpool< http_business >(POOL_THREAD_NUM,MAX_EVENT_NUMBER);
    }
    catch( ... )
    {
        return 1;
    }

    http_business* users = new http_business[ MAX_FD ];
    assert( users );
    int user_count = 0;

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );
    struct linger tmp = { 1, 0 };
    setsockopt( listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof( tmp ) );

    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons( port );

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret >= 0 );

    ret = listen( listenfd, 5 );
    assert( ret >= 0 );

    epoll_event events[ MAX_EVENT_NUMBER ];
    int epollfd_main = epoll_create( 5 );
    assert( epollfd_main != -1 );
    addfd( epollfd_main, listenfd, false );
    http_business::http_epollfd = epollfd_main;

    while( true )
    {
        int number = epoll_wait( epollfd_main, events, MAX_EVENT_NUMBER, -1 );
        if ( ( number < 0 ) && ( errno != EINTR ) )
        {
            printf( "epoll failure\n" );
            break;
        }

        for ( int i = 0; i < number; i++ )
        {
            int sockfd = events[i].data.fd;
            if( sockfd == listenfd )
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                if ( connfd < 0 )
                {
                    printf( "errno is: %d\n", errno );
                    continue;
                }
                if( http_business::http_user_count >= MAX_FD )
                {
                    send_error( connfd, "Internal server busy" );
                    continue;
                }
                
                users[connfd].init( connfd, client_address );
            }
            else if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) )
            {
                users[sockfd].close_conn();
            }
            else if( events[i].events & EPOLLIN )
            {
                if( users[sockfd].read() )
                {
                    pool->append( users + sockfd );
                }
                else
                {
                    users[sockfd].close_conn();
                }
            }
            else if( events[i].events & EPOLLOUT )
            {
                if( !users[sockfd].write() )
                {
                    users[sockfd].close_conn();
                }
            }
            else
            {}
        }
    }

    close( epollfd_main );
    close( listenfd );
    delete [] users;
    delete pool;
    return 0;
}
