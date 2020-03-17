# OSH-2020-lab1

**袁一玮 PB18000221**

## GitHub 仓库

创建了私有仓库 yuanyiwei/OSH-2020-Labs，并且把助教账户 OSH-2020-TA 加为 collaborator。

## Linux 内核

下载 [Linux Kernel](https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.4.22.tar.xz)，由于国内网络环境，最后选择 [tuna 镜像站](https://mirrors.tuna.tsinghua.edu.cn/kernel/v5.x/linux-5.4.22.tar.xz)。

解压 linux-5.4.22.tar.xz

## initrd 与 init 程序

关于制作 initrd，我选择了用 C 语言的 `mknod()` 调用创建设备文件，

## x86 裸金属 MBR 程序

主体部分采用助教给出的示例代码（Hello, OSH 2020 Lab1!），

## 思考题

- 在使用 `make menuconfig` 调整 Linux 编译选项的时候，你会看到有些选项是使用 `[M]` 标记的。它们是什么意思？在你的 `init` 启动之前的整个流程中它们会被加载吗？如果不，在正常的 Linux 系统中，它们是怎么被加载的？

`[M]` 标记意思是作为模块构建，这些模块并不在 `init` 启动之前的整个流程中加载，会在 Linux Kernel 启动后被 insmod 加载。

- 在「构建 initrd」的教程中我们创建了一个示例的 init 程序。为什么屏幕上输出 "Hello, Linux!" 之后，Linux 内核就立刻 kernel panic 了？

因为主进程 `init` 退出了之后，linux 会挂。

- 为什么我们编写 C 语言版本的 init 程序在编译时需要静态链接？我们能够在这里使用动态链接吗？

静态链接把调用的函数都集成到了域程序里，而动态链接依赖于系统内置或用户引入的库文件。

- 在 Vlab 平台上，即使是真正的 root 也无法使用 mknod 等命令，查找资料，尝试简要说明为什么 fakeroot 环境却能够正常使用这些命令。

因为 fakeroot 在看起来具有文件操作的 root 特权的环境中运行命令。

- MBR 能够用于编程的部分只有 510 字节，而目前的系统引导器（如 GRUB2）可以实现很多复杂的功能：如从不同的文件系统中读取文件、引导不同的系统启动、提供美观的引导选择界面等，用 510 字节显然是无法做到这些功能的。它们是怎么做到的？

510 字节的 MBR 恰好包含了足够的只读文件系统的代码，从目录中读入真正的系统引导器（如 GRUB2）。

- 目前，越来越多的 PC 使用 UEFI 启动。请简述使用 UEFI 时系统启动的流程，并与传统 BIOS 的启动流程比较。

启动时 UEFI 使用的是 UEFI 规范，执行 UEFI 的 API 和服务，而 BIOS 使用的是中断。UEFI 相比传统 BIOS 的安全性更好，提供了 SecureBoot 和 GUID 分区表支持。在启动时，UEFI 系统引导启动 EFI 分区下的 .EFI 引导文件（UEFI 默认提供了对 FAT 分区的支持），而 BIOS 则使用磁盘上的 MBR 启动系统。