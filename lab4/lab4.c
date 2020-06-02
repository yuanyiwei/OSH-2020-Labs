#define _GNU_SOURCE // Required for enabling clone(2)
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>     // For clone(2)
#include <signal.h>    // For SIGCHLD constant
#include <sys/mman.h>  // For mmap(2)
#include <sys/types.h> // For wait(2)
#include <sys/wait.h>  // For wait(2)
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>
#define STACK_SIZE (1024 * 1024) // 1 MiB
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
	//cg & dev
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
	if (mkdir("/sys/fs/cgroup/memory", 0777) == -1)
		error_exit(errorcgroup, "error mkdir cg\n");
	if (mkdir("/sys/fs/cgroup/cpu,cpuacct", 0777) == -1)
		error_exit(errorcgroup, "error mkdir cg\n");
	if (mkdir("/sys/fs/cgroup/pids", 0777) == -1)
		error_exit(errorcgroup, "error mkdir cg\n");
	if (mount("cgroup", "/sys/fs/cgroup/pids", "cgroup", MS_NOEXEC | MS_NOSUID, "pids") == -1)
		error_exit(errorcgroup, "error mkdir cg\n");
	if (mount("cgroup", "/sys/fs/cgroup/memory", "cgroup", MS_NOEXEC | MS_NOSUID, "memory") == -1)
		error_exit(errorcgroup, "error mkdir cg\n");
	if (mount("cgroup", "/sys/fs/cgroup/cpu,cpuacct", "cgroup", MS_NOEXEC | MS_NOSUID, "cpu,cpuacct") == -1)
		error_exit(errorcgroup, "error mkdir cg\n");

	if (mount("tsys", "/sys", "sysfs", MS_REMOUNT | MS_RDONLY | MS_NOEXEC | MS_NOSUID, 0) == -1) // ?
		error_exit(errormountfs, "error mount readonly sys\n");

	execvp(((char **)arg)[0], (char *const *)arg);
	error_exit(255, "exec");
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		fprintf(stderr, usage, argv[0]);
		return 1;
	}
	if (chdir(argv[1]) == -1)
		error_exit(1, argv[1]);

	void *child_stack = mmap(NULL,														  //addr为NULL，则内核选择（页面对齐的起始）地址
				 STACK_SIZE,													  // 页面大小
				 PROT_READ | PROT_WRITE,											  //页面可读可写
				 MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,									  //私有 不以任何文件为基础 线程堆栈
				 -1, 0);													  //-1是因为 MAP_ANONYMOUS 否则应为文件句柄
	void *child_stack_start = child_stack + STACK_SIZE;											  //起始指针倒着增长
	int ch = clone(child, child_stack_start, CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWCGROUP | SIGCHLD, argv + 2); //child：子进程的main函数  child_stack_start子进程起始地址 SIGCHLD(wait) name进程参数

	// Parent waits for child
	int status, ecode = 0;
	wait(&status);
	printf("Child exited with status %d\n", WEXITSTATUS(status));
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
