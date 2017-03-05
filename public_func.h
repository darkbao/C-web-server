#ifndef PUBLIC_FUNC_H_
#define PUBLIC_FUNC_H_

int addfd(int epollfd, int fd, bool one_shot);
void modfd(int epollfd, int fd, int ev);
int removefd(int epollfd, int fd);
void addsig(int sig, void(handler)(int), bool restart = true);
void send_error(int connfd, const char* info);
int setnonblocking(int fd);

#endif