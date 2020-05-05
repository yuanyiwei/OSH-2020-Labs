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
	int len;
	int id;
};
int fd, sup_fd;
int user_occupy[max_client];
int privmsg_pos[max_client];
char message[max_buffer];
char tmpbuf[max_buffer];
char privbuf[max_client][max_buffer];
struct sockinfo userinfo[max_client];
queue<messageSent> clientQueue[max_client];
int handle_chat_recv(int i)
{
	memset(message, 0, max_buffer);
	int len;
	while ((len = recv(userinfo[i].sock, privbuf[i] + privmsg_pos[i], max_buffer - privmsg_pos[i], 0)) > 0)
	{
		int pos = 0, sig = 0;
		while (sig < len + privmsg_pos[i])
		{
			if (privbuf[i][sig] == '\n')
			{
				int recvlen = 0;
				for (int j = pos; j < sig; j++)
				{
					message[recvlen] = privbuf[i][j];
					recvlen++;
				}
				struct messageSent recvMsg;
				char *sendmsg = (char *)malloc(max_buffer * sizeof(char));
				memcpy(sendmsg, message, max_buffer * sizeof(char));
				recvMsg.id = i;
				recvMsg.msg = sendmsg;
				recvMsg.len = recvlen;

				printf("user %d send msg %s\n", i, sendmsg);
				fflush(0);

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
				pos = sig + 1;
			}
			sig++;
		}
		privmsg_pos[i] = len - pos + privmsg_pos[i];
		if (privmsg_pos[i] == 0)
		{
			break;
		}
		memcpy(tmpbuf, privbuf[i] + pos, privmsg_pos[i]);
		memcpy(privbuf[i], tmpbuf, privmsg_pos[i]);
	}
	return 0;
}
int handle_chat_send(int i)
{
	memset(message, 0, max_buffer);
	int headerlen = sprintf(message, "User %d: ", clientQueue[i].front().id);
	struct messageSent dealmsg = clientQueue[i].front();
	for (int i = 0; i < dealmsg.len; i++)
	{
		message[headerlen + i] = dealmsg.msg[i];
	}
	message[headerlen + dealmsg.len] = '\n';

	printf("user %d recv msg %s", i, message);
	fflush(0);

	int sig = headerlen + dealmsg.len + 1;
	int send_len = send(userinfo[i].sock, message, sig, 0);
	while (send_len < sig)
	{
		int remain = send(userinfo[i].sock, message + send_len, sig - send_len, 0);
		send_len += remain;
		usleep(1000);
	}
	clientQueue[i].pop();
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
	memset(user_occupy, 0, max_client);
	memset(privbuf, 0, max_client * max_buffer);
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
		FD_SET(fd, &recvclient);
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
				}
				else
				{
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
					struct messageSent welcome;
					sprintf(msgwelcome, "Welcome! You are user%d.\n", thisid);
					welcome.id = 233;
					welcome.msg = msgwelcome;
					welcome.len = strlen(msgwelcome);
					clientQueue[thisid].push(welcome);
					FD_SET(userinfo[thisid].sock, &sendclient);
				}
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