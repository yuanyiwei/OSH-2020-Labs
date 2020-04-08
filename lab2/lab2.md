# OSH-2020-lab2

**袁一玮 PB18000221**

本 Shell 实现了「管道」（`env | grep totoro | wc`）、「基本文件重定向」（`wc < lab2.md > tmp`）、「`^C` 丢弃当前命令」的几个基本任务，模仿 `Oh-My-Zsh` 对命令行进行染色，并增强对空格敏感、错误检查之类的鲁棒性。

一些功能：
- `cd` 无参数时，默认为 `cd ~`

## 参考

[dash](https://git.kernel.org/pub/scm/utils/dash/dash)