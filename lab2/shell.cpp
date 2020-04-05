#include "sh.h"

int result;
char argvs[BUFF_SZ][BUFF_SZ];

int ELF_(const char *cmd)
{
	if (cmd == 0 || strlen(cmd) == 0)
	{
		return 0;
	}
	int END_ = 1, fSTD[2];
	if (pipe(fSTD) == -1)
	{
		END_ = 0;
	}
	else
	{
		int inSTD = dup(STDIN_FILENO);
		int outSTD = dup(STDOUT_FILENO);
		int pid = vfork();
		if (pid == -1)
		{
			END_ = 0;
		}
		else if (pid == 0)
		{
			close(fSTD[0]);
			dup2(fSTD[1], STDOUT_FILENO);
			close(fSTD[1]);
			char buf[BUFF_SZ];
			sprintf(buf, "command -v %s", cmd);
			system(buf);
			exit(1);
		}
		else
		{
			waitpid(pid, 0, 0);
			close(fSTD[1]);
			dup2(fSTD[0], STDIN_FILENO);
			close(fSTD[0]);
			if (getchar() == EOF)
			{
				END_ = 0;
			}
			dup2(inSTD, STDIN_FILENO);
			dup2(outSTD, STDOUT_FILENO);
		}
	}
	return END_;
}
int spCMD(char cmd[BUFF_SZ])
{
	int i, j, k = 0, len = strlen(cmd);
	for (i = 0, j = 0; i < len; i++)
	{
		if (cmd[i] != ' ')
		{
			argvs[k][j++] = cmd[i];
		}
		else
		{
			if (j != 0)
			{
				argvs[k][j] = '\0'; // PIPE x|y
				k++;
				j = 0;
			}
		}
	}
	if (j != 0)
	{
		argvs[k][j] = '\0';
		k++;
	}
	return k;
}
int RED_CMD(int left, int right)
{
	if (!ELF_(argvs[left]))
	{
		return ERROR_COMMAND;
	}
	int l = 0, r = 0, rr = 0, endIndex = right;
	char *inFile = 0, *outFile = 0;

	for (int i = left; i < right; i++)
	{
		if (strcmp(argvs[i], _RIN) == 0)
		{
			l++;
			if (i + 1 < right)
			{
				inFile = argvs[i + 1];
			}
			else
			{
				return ERROR_MISS_PARAMETER;
			}
			if (endIndex == right)
			{
				endIndex = i;
			}
		}
		else if (strcmp(argvs[i], _ROUT) == 0)
		{
			r++;
			if (i + 1 < right)
			{
				outFile = argvs[i + 1];
			}
			else
			{
				return ERROR_MISS_PARAMETER;
			}
			if (endIndex == right)
			{
				endIndex = i;
			}
		}
		else if (strcmp(argvs[i], _RROUT) == 0)
		{
			rr++;
			if (i + 1 < right)
			{
				outFile = argvs[i + 1];
			}
			else
			{
				return ERROR_MISS_PARAMETER;
			}
			if (endIndex == right)
			{
				endIndex = i;
			}
		}
	}
	if (l == 1)
	{
		FILE *fp = fopen(inFile, "r");
		if (fp == 0)
		{
			return ERROR_IN;
		}
		fclose(fp);
	}
	if (l > 1)
	{
		return ERROR_IN;
	}
	else if (r > 1)
	{
		return ERROR_OUT;
	}
	else if (rr > 1)
	{
		return ERROR_OUT;
	}
	int END_ = OK;
	int pid = vfork();
	if (pid == -1)
	{
		END_ = ERROR_FORK;
	}
	else if (pid == 0)
	{
		if (l == 1)
		{
			freopen(inFile, "r", stdin);
		}
		if (r == 1)
		{
			freopen(outFile, "w", stdout);
		}
		else if (rr == 1)
		{
			freopen(outFile, "aw", stdout);
		}
		char *buf[BUFF_SZ];
		for (int i = left; i < endIndex; i++)
		{
			buf[i] = argvs[i];
		}
		buf[endIndex] = 0;
		execvp(buf[left], buf + left);
		exit(errno);
	}
	else
	{
		int status;
		waitpid(pid, &status, 0);
		int err = WEXITSTATUS(status);
		if (err)
		{
			printf("%s\n", strerror(err));
		}
	}
	return END_;
}
int PIPE_CMD(int left, int right)
{
	if (left >= right)
	{
		return OK;
	}
	int pipeIndex = -1;
	for (int i = left; i < right; i++)
	{
		if (strcmp(argvs[i], _PIPE) == 0)
		{
			pipeIndex = i;
			break;
		}
	}
	if (pipeIndex == -1)
	{
		return RED_CMD(left, right);
	}
	else if (pipeIndex + 1 == right)
	{
		return ERROR_PIPE_MISS_PARAMETER;
	}
	int fSTD[2];
	if (pipe(fSTD) == -1)
	{
		return ERROR_PIPE;
	}
	int END_ = OK;
	int pid = vfork();
	if (pid == -1)
	{
		END_ = ERROR_FORK;
	}
	else if (pid == 0)
	{
		close(fSTD[0]);
		dup2(fSTD[1], STDOUT_FILENO);
		close(fSTD[1]);
		END_ = RED_CMD(left, pipeIndex);
		exit(END_);
	}
	else
	{
		int status;
		waitpid(pid, &status, 0);
		int exitCode = WEXITSTATUS(status);
		if (exitCode != OK)
		{
			char info[4096] = {0};
			char line[BUFF_SZ];
			close(fSTD[1]);
			dup2(fSTD[0], STDIN_FILENO);
			close(fSTD[0]);
			while (fgets(line, BUFF_SZ, stdin) != 0)
			{
				strcat(info, line);
			}
			printf("%s\n", info);
			END_ = exitCode;
		}
		else if (pipeIndex + 1 < right)
		{
			close(fSTD[1]);
			dup2(fSTD[0], STDIN_FILENO);
			close(fSTD[0]);
			END_ = PIPE_CMD(pipeIndex + 1, right);
		}
	}
	return END_;
}
int EXIT_()
{
	int pid = getpid();
	if (kill(pid, SIGTERM) == -1)
	{
		return ERROR_EXIT;
	}
	else
	{
		return OK;
	}
}
int CALL_(int commandNum)
{
	int pid = fork();
	if (pid == -1)
	{
		return ERROR_FORK;
	}
	else if (pid == 0)
	{
		int inFds = dup(STDIN_FILENO);
		int outFds = dup(STDOUT_FILENO);
		int END_ = PIPE_CMD(0, commandNum);
		dup2(inFds, STDIN_FILENO);
		dup2(outFds, STDOUT_FILENO);
		exit(END_);
	}
	else
	{
		int status;
		waitpid(pid, &status, 0);
		return WEXITSTATUS(status);
	}
}
int EXPORT_() // bugs
{
	for (int i = 1; argvs[i] != 0; i++)
	{
		char *name = argvs[i];
		char *value = argvs[i] + 1;
		while (*value != '\0' && *value != '=')
		{
			value++;
		}
		*value = '\0';
		value++;
		setenv(name, value, 1);
	}
}
void sighandler(int signum)
{
	// keep alive, todo ctrl+d
}
int main()
{
	signal(SIGINT, sighandler);
	char argv[BUFF_SZ];
	while (1)
	{
		printf("➜  ");
		fgets(argv, BUFF_SZ, stdin);
		int len = strlen(argv);
		if (len != BUFF_SZ)
		{
			argv[len - 1] = '\0';
		}
		int argc = spCMD(argv);
		if (argc != 0)
		{
			if (strcmp(argvs[0], "exit") == 0)
			{
				result = EXIT_();
				if (result == ERROR_EXIT)
				{
					exit(-1);
				}
			}
			else if (strcmp(argvs[0], "export") == 0)
			{
				result = EXPORT_();
				if (result == ERROR_COMMAND)
				{
					fprintf(stderr, "Error export\n");
				}
			}
			else if (strcmp(argvs[0], _CD) == 0)
			{
				if (argc < 2)
				{
					result = ERROR_MISS_PARAMETER;
					fprintf(stderr, "Error miss parameter\n");
				}
				else if (argc > 2)
				{
					result = ERROR_TOO_MANY_PARAMETER;
					fprintf(stderr, "Error too many parameter\n");
				}
				else
				{
					if (chdir(argvs[1]))
					{
						result = ERROR_WRONG_PARAMETER;
						fprintf(stderr, "Error wrong parameter\n");
					}
					else
					{
						result = OK;
					}
				}
			}
			else
			{
				result = CALL_(argc);
				switch (result)
				{
				case ERROR_FORK:
					fprintf(stderr, "Error fork\n");
					exit(ERROR_FORK);
				case ERROR_COMMAND:
					fprintf(stderr, "Error command\n");
					break;
				case ERROR_IN:
					fprintf(stderr, "Error in redirection\n");
					break;
				case ERROR_OUT:
					fprintf(stderr, "Error out redirection\n");
					break;
				case ERROR_MISS_PARAMETER:
					fprintf(stderr, "Error redirect parameter\n");
					break;
				case ERROR_PIPE:
					fprintf(stderr, "Error pipe\n");
					break;
				case ERROR_PIPE_MISS_PARAMETER:
					fprintf(stderr, "Error pipe parameter\n");
					break;
				}
			}
		}
	}
}