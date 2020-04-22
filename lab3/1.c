#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define buf_size 1024
struct Pipe
{
    int fd_send;
    int fd_recv;
};
enum
{
    middle = 0,
    end
};

void *handle_chat(void *data)
{
    int stat = end;
    struct Pipe *pipe = (struct Pipe *)data;
    char buffer[buf_size];
    int len;
    while (1)
    {
        if (stat == middle)
        {
            len = recv(pipe->fd_send, buffer, buf_size, 0);
            if (len <= 0)
            {
                perror("ERROR recv");
                exit(-1);
            }
            else if (len < buf_size)
            {
                buffer[len] = '\n';
                stat = end;
            }
        }
        else if (stat == end)
        {
            strcpy(buffer, "Message:");
            len = recv(pipe->fd_send, buffer + 8, buf_size - 8, 0);
            len += 8;
            if (len - 8 <= 0)
            {
                perror("ERROR recv");
                exit(-1);
            }
            else if (len < buf_size)
            {
                buffer[len] = '\n';
            }
            else
            {
                stat = middle;
            }
        }
        else
        {
            perror("ERROR stat");
            exit(-1);
        }

        int sentlen = send(pipe->fd_recv, buffer, len, 0);
        if (sentlen < len)
        {
            perror("ERROR send");
            exit(-1);
        }
    }
    return 0;
}
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("USAGE: %s port", argv[0]);
        return -1;
    }
    int port = atoi(argv[1]);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
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
        perror("ERROR bind");
        return -1;
    }
    if (listen(fd, 2))
    {
        perror("ERROR listen 2");
        return -1;
    }
    int fd1 = accept(fd, 0, 0);
    int fd2 = accept(fd, 0, 0);
    if (fd1 == -1 || fd2 == -1)
    {
        perror("ERROR accept");
        return -1;
    }
    pthread_t thread1, thread2;
    struct Pipe pipe1;
    struct Pipe pipe2;
    pipe1.fd_send = fd1;
    pipe1.fd_recv = fd2;
    pipe2.fd_send = fd2;
    pipe2.fd_recv = fd1;
    pthread_create(&thread1, 0, handle_chat, (void *)&pipe1);
    pthread_create(&thread2, 0, handle_chat, (void *)&pipe2);
    pthread_join(thread1, 0);
    pthread_join(thread2, 0);
    return 0;
}
