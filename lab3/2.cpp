#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#define max_client 32
#define buf_size 1024
#define send_buff_size 100

struct Pipe
{
    int fd_send;
    int fd_recv;
};
struct sockinfo
{
    int sock;
    int port;
};
enum
{
    middle = 0,
    end
};
char message[1048576];
struct sockinfo thisinfo, userinfo[max_client - 1];
int sock_user[max_client - 1];
pthread_t ptid[max_client - 1] = {};
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int fd;
void *handleChat(void *data)
{
    struct sockinfo *pipe = (sockinfo *)data;
    int stat = end;
    int socks = pipe->sock;
    int ports = pipe->port;
    char receivebuffer[send_buff_size];
    int length = sizeof(sock_user) / sizeof(sock_user[0]);
    int sig;

    while (1)
    {
        sig = 0;
        memset(receivebuffer, 0, send_buff_size);
        int len = recv(socks, receivebuffer, send_buff_size, 0);
        pthread_mutex_lock(&mutex);
        if (len <= 0)
        {
            return 0;
        }

        memset(message, 0, 1048576);
        if (stat == end)
        {
            strcat(message, "Message:");
            sig += 8;
        }
        for (int i = 0; i < len; i++)
        {
            stat = middle;
            if (receivebuffer[i] == '\n')
            {
                stat = end;
            }
            strcpy(message + sig, receivebuffer + i);
            sig++;
        }
        for (int i = 0; i < length; i++)
        {
            if (sock_user[i] != socks)
            {
                send(sock_user[i], message, send_buff_size + 20, 0);
            }
        }
        pthread_mutex_unlock(&mutex);
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
    int usernum = 0;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket");
        return 1;
    }
    struct sockaddr_in serv_addr, client_addr;
    int len = sizeof(client_addr);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(serv_addr);

    if (bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)))
    {
        perror("ERROR bind\n");
        return -1;
    }
    if (listen(fd, 32))
    {
        perror("ERROR listen\n");
        return -1;
    }
    int client_sock;
    while (1)
    {
        client_sock = accept(fd, (struct sockaddr *)&client_addr, (socklen_t *)&len);
        if (client_sock < 0)
        {
            perror("ERROR accept\n");
            exit(-1);
        }
        thisinfo.sock = client_sock;
        thisinfo.port = client_addr.sin_port;
        userinfo[usernum] = thisinfo;
        sock_user[usernum] = client_sock;
        pthread_create(&ptid[usernum], 0, handleChat, &userinfo[usernum]);
        usernum++;
    }
    close(fd);
    close(client_sock);
    close(fd);
    return 0;
}
