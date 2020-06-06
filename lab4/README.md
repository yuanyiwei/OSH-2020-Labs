# Lab4

PB18000221 袁一玮

## 隔离命名空间

```cpp
int ch = clone(child, child_stack_start, CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWCGROUP | SIGCHLD, argv + 2);
```

把几个命名空间 flag 加入即可。

## 挂载文件系统

先把根挂载为私有：

```cpp
if (mount(0, "/", 0, MS_PRIVATE | MS_REC, 0) == -1)
    error_exit(errorpivotroot, "error mount rootfs\n");
```

之后 bind mount，并 lazy unmount 了旧根。

挂载 `/proc`, `/sys`, `/tmp`, `/dev`，并在 `/dev` 下创建必要的节点（还挂载 `cgroup` 并在其目录下创建挂载三个控制组）：

```cpp
if (mount("tdev", "/dev", "devtmpfs", MS_NOEXEC | MS_NOSUID, 0) == -1)
    error_exit(errormountfs, "error mount dev\n");
if (mount("tproc", "/proc", "proc", MS_NOEXEC | MS_NOSUID, 0) == -1)
    error_exit(errormountfs, "error mount proc\n");
if (mount("tsys", "/sys", "sysfs", MS_NOEXEC | MS_NOSUID, 0) == -1)
    error_exit(errormountfs, "error mount sys\n");
if (mount("ttmpfs", "/tmp", "tmpfs", MS_NOEXEC | MS_NOSUID, 0) == -1)
    error_exit(errormountfs, "error mount tmpfs\n");

mknod("/dev/null", 666 | S_IFCHR, makedev(1, 3));
mknod("/dev/tty", 666 | S_IFCHR, makedev(5, 0));
mknod("/dev/urandom", 666 | S_IFCHR, makedev(1, 9));
mknod("/dev/zero", 666 | S_IFCHR, makedev(1, 5));
```

## 使用 pivot_root 并从主机上隐藏容器的挂载点

过程出现在挂载的中间，先把根放到临时目录 tmpdir 下，尝试 cd 移动目录。之后使用 umount2 将旧根惰性卸载，不影响正在使用该文件系统结构的进程，再使用 `rmdir("/oldroot")` 即可。

```cpp
if (mount(".", tmpdir, "devtmpfs", MS_BIND, 0) == -1)
    error_exit(errorpivotroot, "error bind mount\n");
mkdir(oldrootdir, 0777);
if (syscall(SYS_pivot_root, tmpdir, oldrootdir) == -1)
    error_exit(errorpivotroot, "error pivot_root\n");
if (chdir("/") == -1)
    error_exit(errorpivotroot, "error /\n");
if (umount2("/oldroot", MNT_DETACH))
    error_exit(errorpivotroot, "error umount oldroot\n");
rmdir("/oldroot");
sprintf(oldrootdirinc, "/oldroot%s", tmpdir);
rmdir(oldrootdirinc);
```

## 为容器中的进程移除能力

用 libcap-ng 设置白名单：

```cpp
capng_clear(CAPNG_SELECT_BOTH);
if (capng_updatev(CAPNG_ADD,
                    CAPNG_EFFECTIVE | CAPNG_PERMITTED,
                    CAP_SETPCAP, CAP_MKNOD, CAP_AUDIT_WRITE, CAP_CHOWN, CAP_NET_RAW, CAP_DAC_OVERRIDE, CAP_FOWNER, CAP_FSETID, CAP_KILL, CAP_SETGID, CAP_SETUID, CAP_NET_BIND_SERVICE, CAP_SYS_CHROOT, CAP_SETFCAP, -1) == -1)
    error_exit(errorcapabilities, "error capng update\n");
if (capng_apply(CAPNG_SELECT_BOTH) == -1)
    error_exit(errorcapabilities, "error capng apply\n");
```

## 限制容器的系统调用

先 `seccomp_init` 初始化过滤状态，再用 `seccomp_rule_add` 添加宏 `SCMP_SYS(syscall)` 生成白名单系统调用的调用号，最后 `seccomp_load` 应用过滤规则。

## 使用 cgroup 限制容器中的系统资源使用并在容器中挂载三个 cgroup 控制器

在挂载文件系统时，已经完成要求。根据实验要求（用户态内存上限、内核内存上限、禁用交换空间、设置 CPU 配额、设置 PID 数量上限）创建三个 cgroup 目录并在各个新创建的 cgroup 中设置限制：

```cpp
FILE *fp = 0;
int pid = getpid();
mkdir("/sys/fs/cgroup/memory/test", DEFFILEMODE);
mkdir("/sys/fs/cgroup/cpu,cpuacct/test", DEFFILEMODE);
mkdir("/sys/fs/cgroup/pids/test", DEFFILEMODE);
fp = fopen("/sys/fs/cgroup/memory/test/memory.limit_in_bytes", "w");
fprintf(fp, "%d", 67108864);
fclose(fp);
fp = fopen("/sys/fs/cgroup/memory/test/memory.kmem.limit_in_bytes", "w");
fprintf(fp, "%d", 67108864);
fclose(fp);
fp = fopen("/sys/fs/cgroup/memory/test/memory.swappiness", "w");
fprintf(fp, "%d", 0);
fclose(fp);
fp = fopen("/sys/fs/cgroup/memory/test/cgroup.procs", "w");
fprintf(fp, "%d", pid);
fclose(fp);
fp = fopen("/sys/fs/cgroup/cpu,cpuacct/test/cpu.shares", "w");
fprintf(fp, "%d", 256);
fclose(fp);
fp = fopen("/sys/fs/cgroup/cpu,cpuacct/test/cgroup.procs", "w");
fprintf(fp, "%d", pid);
fclose(fp);
fp = fopen("/sys/fs/cgroup/pids/test/pids.max", "w");
fprintf(fp, "%d", 64);
fclose(fp);
fp = fopen("/sys/fs/cgroup/pids/test/cgroup.procs", "w");
fprintf(fp, "%d", pid);
fclose(fp);
```

清理刚才创建的 cgroup：

```cpp
rmdir("/sys/fs/cgroup/memory/test");
rmdir("/sys/fs/cgroup/cpu,cpuacct/test");
rmdir("/sys/fs/cgroup/pids/test");
```

# 思考题

## 用于限制进程能够进行的系统调用的 seccomp 模块实际使用的系统调用是哪个？用于控制进程能力的 capabilities 实际使用的系统调用是哪个？尝试说明为什么本文最上面认为「该系统调用非常复杂」。

## 当你用 cgroup 限制了容器中的 CPU 与内存等资源后，容器中的所有进程都不能够超额使用资源，但是诸如 htop 等「任务管理器」类的工具仍然会显示主机上的全部 CPU 和内存（尽管无法使用）。查找资料，说明原因，尝试提出一种解决方案，使任务管理器一类的程序能够正确显示被限制后的可用 CPU 和内存（不要求实现）。

# 参考

- https://veritas501.space/2018/05/05/seccomp%E5%AD%A6%E4%B9%A0%E7%AC%94%E8%AE%B0/
