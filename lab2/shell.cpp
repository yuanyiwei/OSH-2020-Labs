#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

int main()
{
	char cmd[256];   //input
	char *args[128]; //args end in NULL
	int i;
	int pid;
	int stat_val;
	int isRedirect;
	int isPipe;
	int isBack;
	int position;
	while (1)
	{
		isRedirect = 0;
		isPipe = 0;
		position = 0;
		/* 提示符 */
		printf("# ");
		fflush(stdin);
		fgets(cmd, 256, stdin);
		i = 0;
		while (cmd[i] != '\n')
		{
			i++;
		}
		cmd[i] = '\0';
		args[0] = cmd;
		for (i = 0; *args[i]; i++)
			for (args[i + 1] = args[i] + 1; *args[i + 1]; args[i + 1]++)
				if (*args[i + 1] == ' ')
				{
					*args[i + 1] = '\0';
					args[i + 1]++;
					break;
				}
		args[i] = NULL;

		for (i = 0; args[i] != NULL; i++)
		{
			if (!strcmp(args[i], "<") || !strcmp(args[i], ">"))
			{
				isRedirect = 1;
				position = i;
			}
			if (!strcmp(args[i], "|"))
			{
				isPipe = 1;
				position = i;
			}
		}
		//a lot of pipe????

		if (!args[0])
			continue;

		if (strcmp(args[0], "cd") == 0)
		{
			if (args[1])
				chdir(args[1]);
			continue;
		}
		if (strcmp(args[0], "pwd") == 0)
		{
			char wd[4096];
			puts(getcwd(wd, 4096));
			continue;
		}
		if (strcmp(args[0], "export") == 0)
		{
			for (i = 1; args[i] != NULL; i++)
			{
				char *name = args[i];
				char *value = args[i] + 1;
				while (*value != '\0' && *value != '=')
					value++;
				*value = '\0';
				value++;
				setenv(name, value, 1);
			}
			continue;
		}
		if (strcmp(args[0], "exit") == 0)
		{
			return 0;
		}
		if (strcmp(args[i - 1], "&") == 0)
		{
			args[i - 1] = NULL;
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
					execvp(args[0], args);
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
			command = &args[position + 1];
			args[position] = NULL;
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
						execvp(args[0], args);
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
		}

		/* 外部命令 */
		pid_t pid = fork();
		if (pid == 0)
		{
			/* 子进程 */
			execvp(args[0], args);
			/* execvp失败 */
			return 255;
		}
		/* 父进程 */
		wait(NULL);
	}
}