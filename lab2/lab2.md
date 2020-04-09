# OSH-2020-lab2

**袁一玮 PB18000221**

## Shell

本 Shell 实现了「管道」（`env | grep totoro | wc`）、「基本文件重定向」（`wc < lab2.md > tmp`）、「`^C` 丢弃当前命令」的几个基本任务，模仿 `Oh-My-Zsh` 对命令行进行染色，并增强对空格敏感、错误检查之类的鲁棒性。

一些功能：
- `cd` 无参数时，默认为 `cd ~`
- 重写函数 `echo`（失去了~~很多~~原有的功能），`echo ~` 转到 `echo $HOME`，`echo ~root` 转到 `echo /root`~~（蜜汁卡边界样例）~~，实现了 `echo $PATH`（对所有环境变量）

## 参考

[dash](https://git.kernel.org/pub/scm/utils/dash/dash)
[bash](https://www.gnu.org/software/bash/manual)
