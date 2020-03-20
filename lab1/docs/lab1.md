# OSH-2020-lab1

**袁一玮 PB18000221**

## GitHub 仓库

创建了私有仓库 yuanyiwei/OSH-2020-Labs，并且把助教账户 OSH-2020-TA 加为 collaborator。

## Linux 内核

下载 [Linux Kernel](https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.4.22.tar.xz)，由于国内网络环境，最后选择 [tuna 镜像站](https://mirrors.tuna.tsinghua.edu.cn/kernel/v5.x/linux-5.4.22.tar.xz)。

解压 linux-5.4.22.tar.xz，先 `make defconfig` 建立默认内核配置，参考 [OSH-2018-lab2](https://github.com/OSH-2018/OSH-2018.github.io/tree/master/2/kernel) `make menuconfig` 去除网络、特殊文件系统、驱动、安全选项、调试选项之类与实验无关内容，并开启 VESA VGA graphics support 选项，裁剪内核得 3,995,008 字节。

## initrd 与 init 程序

关于制作 initrd，我选择了用 C 语言 `mknod()` 的调用创建设备文件（./ttyS0 和 ./fb0）。

使用 `execv("./x", NULL);` 分别切换三个进程，并用 `fork()==0;` 判断当前进程是否为子进程。

在 init 程序最后加上 `while (1);` 以防止 init 进程退出，而使 Linux 内核 kernel panic。最后把 init.c 文件以静态链接编译，在 cpio 下把三个可执行文件和 init 程序打包成 initrd。

## x86 裸金属 MBR 程序

主体部分采用助教给出的示例代码（Hello, OSH 2020 Lab1!）。

先开始打开中断：`sti`。

之后清空屏幕，调整 video mode：

```x86asm
clear:
    mov ax, 03h
    int 10h
```

再之后是打印 `Hello, OSH 2020 Lab1!` 字符串，和示例代码的打印输出部分一样。

计时器，不断读取地址为 0x046c 上的值，和原始的值加 18 后比较，进入轮询（死循环）状态：

```x86asm
wait_:
    mov edx, 0x046c
    mov eax, [edx]
    add eax, 18
    mov ebx, eax
loop:
    mov eax, [edx]
    cmp eax, ebx
    je print_str
    jmp loop
```

## 思考题

- 在使用 `make menuconfig` 调整 Linux 编译选项的时候，你会看到有些选项是使用 `[M]` 标记的。它们是什么意思？在你的 `init` 启动之前的整个流程中它们会被加载吗？如果不，在正常的 Linux 系统中，它们是怎么被加载的？

`[M]` 标记意思是作为模块构建，这些模块并不在 `init` 启动之前的整个流程中加载。在正常的 Linux 系统中，这些模块会在 Linux Kernel 启动后被 insmod 加载。

- 在「构建 initrd」的教程中我们创建了一个示例的 init 程序。为什么屏幕上输出 "Hello, Linux!" 之后，Linux 内核就立刻 kernel panic 了？

init 进程去打开标准输入、标注输出、标准出错输出，然后 0，1，2 分别指向打开的文件，之后所有进程的 0，1，2 文件实际上都是从最开始的 init 进程那里继承而来的。因此主进程 init 退出了之后，Linux 内核会 Kernel Panic。

- 为什么我们编写 C 语言版本的 init 程序在编译时需要静态链接？我们能够在这里使用动态链接吗？

静态链接把调用的函数都集成到了域程序里，而动态链接依赖于系统内置或用户引入的库文件。我们在 initrd 里没有打包库文件，因此我们不能在这里使用动态链接。

- 在 Vlab 平台上，即使是真正的 root 也无法使用 mknod 等命令，查找资料，尝试简要说明为什么 fakeroot 环境却能够正常使用这些命令。

因为 fakeroot 可以在看起来具有文件操作的 root 特权的环境中运行命令。

- MBR 能够用于编程的部分只有 510 字节，而目前的系统引导器（如 GRUB2）可以实现很多复杂的功能：如从不同的文件系统中读取文件、引导不同的系统启动、提供美观的引导选择界面等，用 510 字节显然是无法做到这些功能的。它们是怎么做到的？

510 字节的 MBR 恰好包含了足够的只读文件系统的代码，从目录中读入真正的系统引导器（如 GRUB2）。

- 目前，越来越多的 PC 使用 UEFI 启动。请简述使用 UEFI 时系统启动的流程，并与传统 BIOS 的启动流程比较。

启动时 UEFI 使用的是 UEFI 规范，执行 UEFI 的 API 和服务，而 BIOS 使用的是中断。UEFI 相比传统 BIOS 的安全性更好，提供了 SecureBoot 和 GUID 分区表支持。在启动时，UEFI 系统引导启动 EFI 分区下的 .EFI 引导文件（UEFI 默认提供了对 FAT 分区的支持），而 BIOS 则使用磁盘上的 MBR 启动系统。

## 参考

- [fork and execv](https://www.cnblogs.com/HKUI/articles/9576788.html)
- [system and execv](https://blog.csdn.net/qq_25349629/article/details/78364247)
- [system](https://blog.csdn.net/linluan33/article/details/8097916)
- [asm int 10h](https://www.itzhai.com/assembly-int-10h-description.html)