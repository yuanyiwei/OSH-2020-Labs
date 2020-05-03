#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#define max_client 32
#define buf_size 1024
#define rec_buff_size 100

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
int usernum;
int user_occupy[max_client];
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
    char receivebuffer[rec_buff_size];
    int length = sizeof(sock_user) / sizeof(sock_user[0]);
    int sig;
    while (1)
    {
        sig = 0;
        int len = recv(socks, receivebuffer, rec_buff_size, 0);
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
            if (user_occupy[i])
            {
                if (sock_user[i] != socks)
                {
                    send(sock_user[i], message, sig, 0);
                }
            }
        }
        pthread_mutex_unlock(&mutex);
    }
    return 0;
}
void leave(int id)
{
    user_occupy[id] = 0;
    usernum--;
}
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("USAGE: %s port", argv[0]);
        return -1;
    }
    int port = atoi(argv[1]);
    usernum = 0;
    memset(user_occupy, 0, max_client);
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
        if (usernum >= 32)
        {
            continue;
        }
        for (int i = 0; i < max_client; i++)
        {
            if (!user_occupy[i])
            {
                user_occupy[i] = 1;
                thisinfo.id = i;
                break;
            }
        }

        thisinfo.sock = client_sock;
        thisinfo.port = client_addr.sin_port;
        userinfo[thisinfo.id] = thisinfo;
        sock_user[thisinfo.id] = client_sock;
        pthread_create(&ptid[thisinfo.id], 0, handleChat, &userinfo[thisinfo.id]);
        usernum++;
    }
    close(fd);
    close(client_sock);
    close(fd);
    return 0;
}
