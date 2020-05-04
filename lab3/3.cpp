#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#define max_client 32
#define buf_size 1024
struct Pipe
{
    int fd_send;
    int fd_recv;
};
struct sockinfo
{
    int id;
    int sock;
    int port;
};
enum
{
    middle = 0,
    end
};
int usernum, fd;
int user_occupy[max_client];
char message[1048576];
struct sockinfo thisinfo, userinfo[max_client];
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("USAGE: %s port", argv[0]);
        return -1;
    }
    int port = atoi(argv[1]);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == 0)
    {
        perror("ERROR socket");
        return -1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("ERROR bind\n");
        return -1;
    }
    if (listen(fd, max_client))
    {
        perror("ERROR listen\n");
        return -1;
    }
    /*
    int fd1 = accept(fd, NULL, NULL);
    int fd2 = accept(fd, NULL, NULL);
    if (fd1 == -1 || fd2 == -1)
    {
        perror("accept");
        return 1;
    }
    fd_set clients;
    
    char buffer[1024] = "Message:";
    fcntl(fd1, F_SETFL, fcntl(fd1, F_GETFL, 0) | O_NONBLOCK); // 将客户端的套接字设置成非阻塞
    fcntl(fd2, F_SETFL, fcntl(fd2, F_GETFL, 0) | O_NONBLOCK);
    while (1)
    {
        FD_ZERO(&clients);
        FD_SET(fd1, &clients);
        FD_SET(fd2, &clients);
        if (select(fd2 + 1, &clients, NULL, NULL, NULL) > 0)
        { // 找出可以读的套接字
            if (FD_ISSET(fd1, &clients))
            {
                ssize_t len = recv(fd1, buffer + 8, 1000, 0);
                if (len <= 0)
                {
                    break;
                }
                send(fd2, buffer, len + 8, 0);
            }
            if (FD_ISSET(fd2, &clients))
            {
                ssize_t len = recv(fd2, buffer + 8, 1000, 0);
                if (len <= 0)
                {
                    break;
                }
                send(fd1, buffer, len + 8, 0);
            }
        }
        else
        {
            break;
        }
    }*/
    return 0;
}