#ifndef __MJ_PUBLIC_FUNC_H_
#define __MJ_PUBLIC_FUNC_H_
#include <errno.h>

#define exit_if(r, ...)                                                                          \
    if (r) {                                                                                     \
        printf(__VA_ARGS__);                                                                     \
        printf("%s:%d error no: %d error msg %s\n", __FILE__, __LINE__, errno, strerror(errno)); \
        exit(1);                                                                                 \
    }

namespace mj
{
    void updateEvents(int epollfd, int fd, int ev, int op);
    void removeAndClose(int epollfd, int fd);
    void setNonBlock(int fd);
    void addsig(int sig, void(handler)(int), bool restart = true);
}
#endif