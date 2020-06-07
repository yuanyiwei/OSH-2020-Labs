#define _GNU_SOURCE // Required for enabling clone(2)
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>	   // For clone(2)
#include <signal.h>	   // For SIGCHLD constant
#include <sys/mman.h>  // For mmap(2)
#include <sys/types.h> // For wait(2)
#include <sys/wait.h>  // For wait(2)
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sysmacros.h>
#include <cap-ng.h>
#include <seccomp.h>
#define STACK_SIZE (1024 * 1024)
enum
{
	errorpivotroot = 1,
	errormountfs,
	errorcapabilities,
	errorseccomp,
	errorcgroup
};
const char *usage = "Usage: %s <directory> <command> [args...]\n\nRun <directory> as a container and execute <command>.\n";

void error_exit(int code, const char *message)
{
	perror(message);
	_exit(code);
}
int child(void *arg)
{
	char tmpdir[50] = "/tmp/xyy-XXXXXX";
	char oldrootdir[50];
	char oldrootdirinc[50];
	mkdtemp(tmpdir);
	sprintf(oldrootdir, "%s/oldroot", tmpdir);
	if (mount(0, "/", 0, MS_PRIVATE | MS_REC, 0) == -1)
		error_exit(errorpivotroot, "error mount rootfs\n");

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

	if (mount("tdev", "/dev", "devtmpfs", MS_NOEXEC | MS_NOSUID, 0) == -1)
		error_exit(errormountfs, "error mount dev\n");
	if (mount("tproc", "/proc", "proc", MS_NOEXEC | MS_NOSUID, 0) == -1)
		error_exit(errormountfs, "error mount proc\n");
	if (mount("tsys", "/sys", "sysfs", MS_NOEXEC | MS_NOSUID, 0) == -1)
		error_exit(errormountfs, "error mount sys\n");
	if (mount("ttmpfs", "/tmp", "tmpfs", MS_NOEXEC | MS_NOSUID, 0) == -1)
		error_exit(errormountfs, "error mount tmpfs\n");

	if (mount("tmpfs", "/sys/fs/cgroup", "tmpfs", MS_NOEXEC | MS_NOSUID, NULL) == -1)
		error_exit(errorcgroup, "error mount cg\n");
	if (mkdir("/sys/fs/cgroup/memory", 0755) == -1)
		error_exit(errorcgroup, "error mkdir cg\n");
	if (mkdir("/sys/fs/cgroup/cpu,cpuacct", 0777) == -1)
		error_exit(errorcgroup, "error mkdir cg\n");
	if (mkdir("/sys/fs/cgroup/pids", 0755) == -1)
		error_exit(errorcgroup, "error mkdir cg\n");

	if (mount("cgroup", "/sys/fs/cgroup/pids", "cgroup", MS_NOEXEC | MS_NOSUID, "pids") == -1)
		error_exit(errorcgroup, "error mkdir cg\n");
	if (mount("cgroup", "/sys/fs/cgroup/memory", "cgroup", MS_NOEXEC | MS_NOSUID, "memory") == -1)
		error_exit(errorcgroup, "error mkdir cg\n");
	if (mount("cgroup", "/sys/fs/cgroup/cpu,cpuacct", "cgroup", MS_NOEXEC | MS_NOSUID, "cpu,cpuacct") == -1)
		error_exit(errorcgroup, "error mkdir cg\n");

	mknod("/dev/null", 666 | S_IFCHR, makedev(1, 3));
	mknod("/dev/tty", 666 | S_IFCHR, makedev(5, 0));
	mknod("/dev/urandom", 666 | S_IFCHR, makedev(1, 9));
	mknod("/dev/zero", 666 | S_IFCHR, makedev(1, 5));

	if (mount("tsys", "/sys", "sysfs", MS_REMOUNT | MS_RDONLY | MS_NOEXEC | MS_NOSUID, 0) == -1)
		error_exit(errormountfs, "error mount readonly sys\n");

	capng_clear(CAPNG_SELECT_BOTH);
	if (capng_updatev(CAPNG_ADD,
					  CAPNG_EFFECTIVE | CAPNG_PERMITTED,
					  CAP_SETPCAP, CAP_MKNOD, CAP_AUDIT_WRITE, CAP_CHOWN, CAP_NET_RAW, CAP_DAC_OVERRIDE, CAP_FOWNER, CAP_FSETID, CAP_KILL, CAP_SETGID, CAP_SETUID, CAP_NET_BIND_SERVICE, CAP_SYS_CHROOT, CAP_SETFCAP, -1) == -1)
		error_exit(errorcapabilities, "error capng update\n");
	if (capng_apply(CAPNG_SELECT_BOTH) == -1)
		error_exit(errorcapabilities, "error capng apply\n");

	scmp_filter_ctx ctx;
	ctx = seccomp_init(SCMP_ACT_KILL);
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(accept), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(accept4), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(access), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(adjtimex), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(alarm), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(bind), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(capget), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(capset), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(chdir), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(chmod), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(chown), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(chown32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_getres), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_getres_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_nanosleep), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_nanosleep_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(connect), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(copy_file_range), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(creat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup2), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup3), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_create), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_create1), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_ctl), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_ctl_old), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_pwait), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_wait), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(epoll_wait_old), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(eventfd), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(eventfd2), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execve), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execveat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(faccessat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fadvise64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fadvise64_64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fallocate), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fanotify_mark), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fchdir), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fchmod), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fchmodat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fchown), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fchown32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fchownat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fdatasync), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fgetxattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(flistxattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(flock), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fork), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fremovexattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fsetxattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstatat64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstatfs), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstatfs64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fsync), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ftruncate), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ftruncate64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(futex), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(futex_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(futimesat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getcpu), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getcwd), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getdents), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getdents64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getegid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getegid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(geteuid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(geteuid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgroups), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgroups32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getitimer), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpeername), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpgid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpgrp), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getppid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpriority), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getrandom), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getresgid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getresgid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getresuid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getresuid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getrlimit), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(get_robust_list), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getrusage), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getsid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getsockname), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getsockopt), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(get_thread_area), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(gettid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(gettimeofday), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getuid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getuid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getxattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(inotify_add_watch), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(inotify_init), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(inotify_init1), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(inotify_rm_watch), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(io_cancel), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(io_destroy), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(io_getevents), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(io_pgetevents), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(io_pgetevents_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioprio_get), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioprio_set), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(io_setup), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(io_submit), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ipc), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(kill), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lchown), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lchown32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lgetxattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(link), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(linkat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(listen), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(listxattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(llistxattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(_llseek), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lremovexattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lseek), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lsetxattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lstat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lstat64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(madvise), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(memfd_create), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mincore), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mkdir), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mkdirat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mknod), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mknodat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mlock), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mlock2), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mlockall), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap2), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mq_getsetattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mq_notify), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mq_open), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mq_timedreceive), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mq_timedreceive_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mq_timedsend), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mq_timedsend_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mq_unlink), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mremap), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(msgctl), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(msgget), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(msgrcv), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(msgsnd), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(msync), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munlock), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munlockall), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(nanosleep), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(newfstatat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(_newselect), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pause), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pipe), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pipe2), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(poll), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ppoll), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ppoll_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(prctl), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pread64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(preadv), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(preadv2), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(prlimit64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pselect6), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pselect6_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwrite64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwritev), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwritev2), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(readahead), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(readlink), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(readlinkat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(readv), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(recv), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvfrom), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvmmsg), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvmmsg_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvmsg), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(remap_file_pages), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(removexattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rename), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(renameat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(renameat2), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(restart_syscall), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rmdir), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigaction), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigpending), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigprocmask), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigqueueinfo), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigreturn), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigsuspend), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigtimedwait), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigtimedwait_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_tgsigqueueinfo), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_getaffinity), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_getattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_getparam), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_get_priority_max), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_get_priority_min), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_getscheduler), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_rr_get_interval), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_rr_get_interval_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_setaffinity), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_setattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_setparam), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_setscheduler), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sched_yield), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(seccomp), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(select), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(semctl), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(semget), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(semop), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(semtimedop), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(semtimedop_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(send), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendfile), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendfile64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendmmsg), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendmsg), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendto), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setfsgid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setfsgid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setfsuid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setfsuid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setgid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setgid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setgroups), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setgroups32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setitimer), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setpgid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setpriority), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setregid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setregid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setresgid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setresgid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setresuid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setresuid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setreuid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setreuid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setrlimit), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(set_robust_list), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setsid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setsockopt), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(set_thread_area), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(set_tid_address), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setuid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setuid32), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setxattr), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(shmat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(shmctl), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(shmdt), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(shmget), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(shutdown), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigaltstack), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(signalfd), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(signalfd4), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigprocmask), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigreturn), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(socket), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(socketcall), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(socketpair), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(splice), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(stat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(stat64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(statfs), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(statfs64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(statx), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(symlink), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(symlinkat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sync), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sync_file_range), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(syncfs), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sysinfo), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(tee), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(tgkill), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(time), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timer_create), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timer_delete), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timer_getoverrun), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timer_gettime), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timer_gettime64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timer_settime), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timer_settime64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timerfd_create), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timerfd_gettime), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timerfd_gettime64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timerfd_settime), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(timerfd_settime64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(times), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(tkill), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(truncate), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(truncate64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ugetrlimit), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(umask), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(uname), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(unlink), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(unlinkat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(utime), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(utimensat), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(utimensat_time64), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(utimes), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(vfork), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(vmsplice), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(wait4), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(waitid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(waitpid), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(writev), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(arch_prctl), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(modify_ldt), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(bpf), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clone), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fanotify_init), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lookup_dcookie), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mount), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(name_to_handle_at), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(perf_event_open), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(quotactl), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setdomainname), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sethostname), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setns), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(syslog), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(umount), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(umount2), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(unshare), 0) != 0)
		error_exit(errorseccomp, "seccomp");
	if (seccomp_load(ctx) < 0)
		error_exit(errorseccomp, "seccomp");

	execvp(((char **)arg)[0], (char *const *)arg);
	error_exit(255, "exec");
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		fprintf(stderr, usage, argv[0]);
		return -1;
	}
	if (chdir(argv[1]) == -1)
		error_exit(1, argv[1]);
	FILE *fp = 0;
	void *child_stack = mmap(NULL,									  //addr为NULL，则内核选择（页面对齐的起始）地址
							 STACK_SIZE,							  // 页面大小
							 PROT_READ | PROT_WRITE,				  //页面可读可写
							 MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, //私有 不以任何文件为基础 线程堆栈
							 -1, 0);								  //-1是因为 MAP_ANONYMOUS 否则应为文件句柄
	void *child_stack_start = child_stack + STACK_SIZE;				  //起始指针倒着增长
	int status, ecode = 0;
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

	int ch = clone(child, child_stack_start, CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWCGROUP | SIGCHLD, argv + 2); //child：子进程的main函数  child_stack_start子进程起始地址 SIGCHLD(wait) name进程参数
	if (ch != -1)
	{
		wait(&status);
		printf("Child exited with status %d\n", WEXITSTATUS(status));
	}
	else
		perror("error clone\n");

	rmdir("/sys/fs/cgroup/memory/test");
	rmdir("/sys/fs/cgroup/cpu,cpuacct/test");
	rmdir("/sys/fs/cgroup/pids/test");
	if (WIFEXITED(status))
	{
		printf("Exited with status %d\n", WEXITSTATUS(status));
		ecode = WEXITSTATUS(status);
	}
	else if (WIFSIGNALED(status))
	{
		printf("Killed by signal %d\n", WTERMSIG(status));
		ecode = -WTERMSIG(status);
	}
	return ecode;
}
