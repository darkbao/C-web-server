#ifndef __MJ_PUBLIC_FUNC_H_
#define __MJ_PUBLIC_FUNC_H_

namespace mj
{
    int  addfd(int epollfd, int fd);
    int  removefd(int epollfd, int fd, bool half_close = false);
    int  setnonblocking(int fd);
    void modfd(int epollfd, int fd, int ev);
    void addsig(int sig, void(handler)(int), bool restart = true);
    void send_error(int connfd, const char* info);
}
#endif