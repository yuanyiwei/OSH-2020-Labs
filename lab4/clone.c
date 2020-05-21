#define _GNU_SOURCE // Required for enabling clone(2)
#include <stdio.h>
#include <unistd.h>
#include <sched.h>               // For clone(2)
#include <signal.h>              // For SIGCHLD constant
#include <sys/mman.h>            // For mmap(2)
#include <sys/types.h>           // For wait(2)
#include <sys/wait.h>            // For wait(2)
#define STACK_SIZE (1024 * 1024) // 1 MiB

int child(void *arg)
{
    printf("My name is %s\n", (char *)arg);
    return 3;
}

int main()
{
    char name[] = "child";
    void *child_stack = mmap(NULL,                                    //addr为NULL，则内核选择（页面对齐的起始）地址
                             STACK_SIZE,                              // 页面大小
                             PROT_READ | PROT_WRITE,                  //页面可读可写
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, //私有 不以任何文件为基础 线程堆栈
                             -1, 0);                                  //-1是因为 MAP_ANONYMOUS 否则应为文件句柄
    void *child_stack_start = child_stack + STACK_SIZE;               //起始指针倒着增长
    int ch = clone(child, child_stack_start,
                   CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWCGROUP | SIGCHLD, name); //child：子进程的main函数  child_stack_start子进程起始地址 SIGCHLD(wait) name进程参数
    int status;
    wait(&status);
    printf("Child exited with code %d\n", WEXITSTATUS(status));
    return 0;
}