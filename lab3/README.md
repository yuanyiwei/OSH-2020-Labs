# OSH_Lab3

PB18000221 袁一玮

## 双人聊天室

为了处理 send 的阻塞，选择不断重试发送并在每次发送后等待一小段时间。

```cpp
while (sentlen < sig)
{
    int remain = send(pipe->fd_recv, message + sentlen, sig - sentlen, 0);
    sentlen += remain;
    usleep(1000);
}
```

## 基于多线程的多人聊天室

聊天室用 (int) user_occupy 保存用户的存在信息，使用一个全局的 mutex 来管理锁，每个用户有一个 (struct sockinfo) userinfo 保存套接字。

离开目前还不能用，和当已有消息发送时再加入的用户无法收到发言。

## 基于 IO 复用的多人聊天室

和初始版本相同，main 中的主循环不断 select 来调起 I/O 读写。每次创建收的 fd_set `recvclient` 和写的 fd_set `sendclient`；对于用户也分别 FD_SET 设置。使用 FD_ISSET 检查可收可写权限，尝试 handle_chat_recv 和 handle_chat_send。

因为 I/O 异步，必须要先检查可写权限，用多线程部分的~~全局阻塞~~锁会导致每个用户的状态不一样，故加入了 std::queue。

因为 `性能不是本实验关注的重点`，free 时有多次 free 的冲突，由于时间原因就没写释放函数了。~~看到内存飙升是正常现象~~。

离开目前还不能用，和当已有消息发送时再加入的用户无法收到发言。
