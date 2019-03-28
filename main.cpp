#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "threadpool.hpp"
#include "http_business.h"
using namespace mj;

int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf("usage: %s port_number thread_num\n", basename(argv[0]));
        return -1;
    }
    int port = atoi(argv[1]);
    int thread_num = atoi(argv[2]);
    addsig(SIGPIPE, SIG_IGN);
    threadpool<http_business>* pool = new threadpool<http_business>(thread_num, port);
    if (pool == NULL) {
        printf("init threadpool error\n");
        return -1;
    }
    const pthread_t* threadIDs = pool->getAllThreadID();
    for (int i = 0; i < thread_num; ++i) {
        if (pthread_join(threadIDs[i], NULL)) {
            printf("pthread_join failed\n");
            return -2;
        }
    }
    delete pool;
    return 0;
}
