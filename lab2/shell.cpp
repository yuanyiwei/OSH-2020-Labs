#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>

void sighandler(int signum)
{
	// keep alive
}
const int BUFF_SZ 256;
const int CMD_SZ 128;
enum
{
	RESULT_NORMAL = 0,
	ERROR_FORK,
	ERROR_COMMAND,
	ERROR_TOO_MANY_PARAMETER,
	ERROR_CD,
	ERROR_EXIT,

	// redirect
	ERROR_MANY_IN,
	ERROR_MANY_OUT,

	// Pipe
	ERROR_PIPE,
};

int splitCMD(char cmd[BUFF_SZ])
{
	for (int i = 0; *argvs[i]; i++)
		for (argvs[i + 1] = argvs[i] + 1; *argvs[i + 1]; argvs[i + 1]++)
			if (*argvs[i + 1] == ' ')
			{
				*argvs[i + 1] = '\0';
				argvs[i + 1]++;
				break;
			}
}

int main()
{
	signal(SIGINT, sighandler);
	char cmd[BUFF_SZ];	 //input
	char *argvs[CMD_SZ]; //argvs end in NULL
	int i, pid, stat_val, isRedirect, isPipe, isBack, position, argc;
	while (1)
	{
		isRedirect = 0;
		isPipe = 0;
		position = 0;
		printf("# ");
		fflush(stdin);
		fgets(cmd, BUFF_SZ, stdin);
		i = 0;
		while (cmd[i] != '\n')
		{
			i++;
		}
		cmd[i] = '\0';
		argvs[0] = cmd;
		splitCMD(argvs[0]);
		argvs[i] = NULL;

		for (i = 0; argvs[i] != NULL; i++)
		{
			if (!strcmp(argvs[i], "<") || !strcmp(argvs[i], ">"))
			{
				isRedirect = 1;
				position = i;
			}
			if (!strcmp(argvs[i], "|"))
			{
				isPipe = 1;
				position = i;
			}
		}
		//a lot of pipe????

		if (!argvs[0])
			continue;

		if (strcmp(argvs[0], "cd") == 0)
		{
			if (argvs[1])
				chdir(argvs[1]);
			continue;
		}
		if (strcmp(argvs[0], "pwd") == 0)
		{
			char wd[4096];
			puts(getcwd(wd, 4096));
			continue;
		}
		if (strcmp(argvs[0], "export") == 0)
		{
			for (i = 1; argvs[i] != NULL; i++)
			{
				char *name = argvs[i];
				char *value = argvs[i] + 1;
				while (*value != '\0' && *value != '=')
					value++;
				*value = '\0';
				value++;
				setenv(name, value, 1);
			}
			continue;
		}
		if (strcmp(argvs[0], "exit") == 0)
		{
			return 0;
		}

		if (strcmp(argvs[i - 1], "&") == 0)
		{
			argvs[i - 1] = NULL;
			pid = fork(); //bulid a runback process,and don't eliminate the parent process
			if (pid == -1)
			{
				perror("Failed to fork");
				return 1;
			}
			if (pid == 0)
			{
				if (setsid() == -1)
					perror("Child faild to become a session leader");
				else
				{
					execvp(argvs[0], argvs);
					printf("Command not found in PATH\n");
					exit(0);
				}
				return 1;
			}
			continue;
		}
		if (isPipe)
		{
			pid_t child, gChild;
			int file_pipes[2];
			char **command;
			command = &argvs[position + 1];
			argvs[position] = NULL;
			child = fork();
			switch (child)
			{
			case -1:
				perror("Fork Failed\n");
				exit(-1);

			case 0:
				if (!pipe(file_pipes))
				{
					gChild = fork();
					switch (gChild)
					{
					case -1:
						fprintf(stderr, "Fork Failed\n");
						exit(-1);

					case 0: //the grandchild process,writing to the pipe

						close(1);
						dup2(file_pipes[1], 1);
						close(file_pipes[1]);
						close(file_pipes[0]);
						execvp(argvs[0], argvs);
						printf("Command not found in PATH\n");
						exit(0);

					default: //the child process, reading from the pipe

						close(0);
						dup(file_pipes[0]);
						close(file_pipes[0]);
						close(file_pipes[1]);
						execvp(command[0], command);
						printf("Command not found in PATH\n");
						exit(0);
					}
				}
			default:
				waitpid(child, &stat_val, 0);
				if (!WIFEXITED(stat_val))
					printf("Child terminated abnormally\n");
			}
			continue;
		}

		/* 外部命令 */
		pid_t pid = fork();
		if (pid == 0)
		{
			/* 子进程 */
			execvp(argvs[0], argvs);
			/* execvp失败 */
			return 255;
		}
		/* 父进程 */
		wait(NULL);
	}
}