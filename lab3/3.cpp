#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <queue>
using std::queue;
#define max_client 32
#define buf_size 1024
#define max_buffer 1048576

struct sockinfo
{
	int id;
	int sock;
};
struct messageSent
{
	char *msg;
	long len;
	int id;
};
int fd, sup_fd;
int user_occupy[max_client];
int privmsg_pos[max_client];
char message[max_buffer];
char bufferl[max_buffer];
char clientMessage[max_client][max_buffer];
struct sockinfo userinfo[max_client];
queue<messageSent> clientQueue[max_client];
int handle_chat_recv(int i)
{
	int len;
	while ((len = recv(userinfo[i].sock, message + privmsg_pos[i], max_buffer - privmsg_pos[i], 0)) > 0)
	{
		int pos = 0, sig = 0;
		while (sig < len + privmsg_pos[i])
		{
			sig++;
			if (message[sig] == '\n')
			{
				char recvmsg[buf_size];
				int recvlen = 0;
				for (int j = pos; j <= sig; j++)
				{
					recvmsg[recvlen] = message[j];
					recvlen++;
				}
				messageSent recvMsg;
				recvMsg.id = i;
				recvMsg.msg = recvmsg;
				recvMsg.len = recvlen;
				for (int socks = 0; socks < max_client; socks++)
				{
					if (user_occupy[socks])
					{
						if (socks != i)
						{
							clientQueue[socks].push(recvMsg);
						}
					}
				}
				pos = sig;
			}
		}
		privmsg_pos[i] = len - pos + privmsg_pos[i];
		if (privmsg_pos[i] == 0)
		{
			break;
		}
		memcpy(bufferl, message + pos, privmsg_pos[i]);
		memcpy(message, bufferl, privmsg_pos[i]);
	}
	if (len <= 0)
	{
		user_occupy[i] = 0;
	}
}
int handle_chat_send(int i)
{

	char header[buf_size];
	int headerlen = sprintf(header, "User %d: ", clientQueue[i].front().id);
	memcpy(message, header, headerlen);
	messageSent dealmsg = clientQueue[i].front();
	for (int i = 0; i < dealmsg.len; i++)
	{
		message[headerlen + i] = dealmsg.msg[i];
	}
	int send_len = send(userinfo[i].sock, message, headerlen + dealmsg.len, 0);
	if (send_len == headerlen + dealmsg.len)
	{
		clientQueue[i].pop();
	}
	else
	{
		char new_msg[buf_size];
		int new_msg_len = 0;
		int start_pos;
		if (send_len < headerlen)
		{
			start_pos = 0;
		}
		else
		{
			start_pos = send_len - headerlen;
		}
		clientQueue[i].front().msg = dealmsg.msg + start_pos;
		clientQueue[i].front().len = dealmsg.len - start_pos;
	}
}
int main(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("USAGE: %s port", argv[0]);
		return -1;
	}
	int port = atoi(argv[1]);
	memset(user_occupy, 0, max_client);
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == 0)
	{
		perror("ERROR socket");
		return -1;
	}
	sup_fd = fd + 1;
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
	fd_set recvclient, sendclient;
	while (1)
	{
		FD_ZERO(&recvclient);
		FD_ZERO(&sendclient);
		for (int i = 0; i < max_client; i++)
		{
			if (user_occupy[i])
			{
				FD_SET(userinfo[i].sock, &recvclient);
				if (clientQueue[i].empty() == 0)
				{
					FD_SET(userinfo[i].sock, &sendclient);
				}
			}
		}
		FD_SET(fd, &recvclient);
		if (select(sup_fd, &recvclient, &sendclient, NULL, NULL) > 0)
		{
			if (FD_ISSET(fd, &recvclient))
			{
				int newfd = accept(fd, NULL, NULL);
				if (newfd == -1)
				{
					perror("ERROR accept\n");
					return -2;
				}
				int thisid = -1;
				for (int i = 0; i < max_client; i++)
				{
					if (user_occupy[i] == 0)
					{
						thisid = i;
						break;
					}
				}
				if (thisid == -1)
				{
					perror("ERROR usernum\n");
					close(newfd);
					continue;
				}
				userinfo[thisid].sock = newfd;
				userinfo[thisid].id = thisid;
				if (userinfo[thisid].sock > sup_fd)
				{
					sup_fd = userinfo[thisid].sock + 1;
				}
				user_occupy[thisid] = 1;
				privmsg_pos[thisid] = 0;
				fcntl(userinfo[thisid].sock, F_SETFL, fcntl(userinfo[thisid].sock, F_GETFL, 0) | O_NONBLOCK);
				char msgwelcome[buf_size];
				messageSent welcome;
				sprintf(msgwelcome, "Welcome! You are user%d.\n", thisid);
				welcome.id = 233;
				welcome.msg = msgwelcome;
				welcome.len = strlen(msgwelcome);
				clientQueue[thisid].push(welcome);
				continue;
			}
			for (int i = 0; i < max_client; i++)
			{
				if (user_occupy[i])
				{
					if (FD_ISSET(userinfo[i].sock, &recvclient))
					{
						handle_chat_recv(i);
					}
					else if (FD_ISSET(userinfo[i].sock, &sendclient))
					{
						handle_chat_send(i);
					}
				}
			}
		}
	}
	return 0;
}