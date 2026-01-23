#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#define PATH_MAX_LEN 1024
#define MAX_CMD_LEN 1024
#define MAX_ARGS 64

void type_prompt()
{
	char cwd[PATH_MAX_LEN];

	if (getcwd(cwd, sizeof(cwd)) != NULL)
	{
		printf("[%s] $ ", cwd);
		fflush(stdout);
	} 
	else 
	{
		perror("getcwd error");
	}
}	

int read_command(char *buffer)
{
	if (fgets(buffer, MAX_CMD_LEN, stdin) == NULL)
	{
		return 0;
	}
	buffer[strcspn(buffer, "\n")] = '\0';
	return 1;
}

int parse_command(char *input, char **args)
{
	int i = 0;
	char *token = strtok(input, " \t");

	while (token != NULL && i < MAX_ARGS - 1)
	{
		args[i] = token;
		i++;
		token = strtok(NULL, " \t");
	}

	args[i] = NULL;
	return i;
}

void builtin_cd(char **args)
{
	char *path;

	if (args[1] == NULL)
	{
		path = getenv("HOME");
		if (path == NULL)
		{
			fprintf(stderr, "cd: HOME variable not set\n");
			return;
		}
	} 
	else 
	{
		path = args[1];
	}

	if (chdir(path) != 0)
	{
		perror("cd");
	}
}

void builtin_help()
{
	printf("\n--- Microshell by Author ---\n");
	printf("Wbudowany komendy:\n");
	printf(" cd [path] - zmienić katlog");
	printf(" exit - wyjść z programu\n");
	printf(" help - wyświetlić ten komunikat\n\n");
}

void execute_external(char **args)
{
	pid_t pid;
	int status;

	pid = fork();

	if (pid == 0)
	{
		if (execvp(args[0], args) == -1)
		{
			perror("microshell");
		}
		exit(EXIT_FAILURE);
	} 
	else if (pid < 0)
	{
		perror("fork failed");
	}
	else
	{
		waitpid(pid, &status, 0);
	}
}

int execute_command(char **args) 
{
	if (args[0] == NULL)
	{
		return 1;
	}

	if (strcmp(args[0], "exit") == 0)
	{
		return 0;
	}

	if (strcmp(args[0], "cd") == 0)
	{
		builtin_cd(args);
		return 1;
	}

	if (strcmp(args[0], "help") == 0)
	{
		builtin_help();
		return 1;
	}

	execute_external(args);

	return 1;
}

int main()
{
	char input_buffer[MAX_CMD_LEN];
	char *args[MAX_ARGS];
	int status = 1;

	while (status)
	{
		type_prompt();

		if (read_command(input_buffer) == 0)
		{
			printf("\n");
			break;
		}

		parse_command(input_buffer, args);

		status = execute_command(args);
	}

	return 0;
}