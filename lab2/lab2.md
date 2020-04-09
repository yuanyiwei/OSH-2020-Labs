# OSH-2020-lab2

**袁一玮 PB18000221**

## Shell

本 Shell 实现了「管道」（`env | grep totoro | wc`）、「基本文件重定向」（`wc < lab2.md > tmp`）、「`^C` 丢弃当前命令」的几个基本任务，模仿 `Oh-My-Zsh` 对命令行进行染色，并增强对空格敏感、错误检查之类的鲁棒性。

一些功能：
- `cd` 无参数时，默认为 `cd ~`
- 重写函数 `echo`（失去了~~很多~~原有的功能），`echo ~` 转到 `echo $HOME`，`echo ~root` 转到 `echo /root`~~（蜜汁卡边界样例）~~，实现了 `echo $PATH`（对所有环境变量）

strace 查看系统调用：
- `WIFEXITED`（宏），检测 `wait` 或 `waitpid` 函数返回其状态的子进程是否正常退出，若是正常退出，则 `WIFEXITED` 宏的计算结果为 `TRUE`
- `pread`，从文件描述符 fd 中以偏移量 offset 开始读取最多计数的字节，并从 buf 开始将其计数到缓冲区中
- `mprotect`，为所指定的内存页进行保护，禁止其他程序访问并给出 SIGSEGV 信号
- `mmap`，把对象映射在虚拟地址空间里的页上

## 参考

[dash](https://git.kernel.org/pub/scm/utils/dash/dash)
[bash](https://www.gnu.org/software/bash/manual)
