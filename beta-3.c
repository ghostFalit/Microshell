#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

#define PATH_MAX_LEN 1024
#define MAX_CMD_LEN 1024
#define MAX_ARGS 64

#define C_RED 		"\033[1;31m"
#define C_GREEN		"\033[1;32m"
#define C_BLUE		"\033[1;34m"
#define C_RESET 	"\033[0m"

void type_prompt()
{
	char cwd[PATH_MAX_LEN];
	char *user;

	user = getenv("USER");
	if (user == NULL)
	{
		user = "unknown";
	}

	if (getcwd(cwd, sizeof(cwd)) != NULL)
	{
		printf("[%s%s" C_RESET ":" C_BLUE "%s" C_RESET "] $ ", C_RED, user, cwd);
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
	printf(" Wbudowany komendy:\n");
	printf(" * cd [path] - zmienić katlog\n");
	printf(" * exit - wyjść z programu\n");
	printf(" * help - wyświetlić ten komunikat\n");
	printf(" * Dodatkowe bajery: login, kolory, CTRL+C\n\n");
}

void execute_external(char **args)
{
	pid_t pid;
	int status;

	pid = fork();

	if (pid == 0)
	{
		signal(SIGINT, SIG_DFL);

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

void setup_signals()
{
	signal(SIGINT, SIG_IGN);
}

int main()
{
	char input_buffer[MAX_CMD_LEN];
	char *args[MAX_ARGS];
	int status = 1;

	setup_signals();

	while (status)
	{
		type_prompt();

		if (read_command(input_buffer) == 0)
		{
			if (feof(stdin))
			{
				printf("\nBye!\n");
				break;
			}

			clearerr(stdin);
			printf("\n");
			continue;
		}

		parse_command(input_buffer, args);
		status = execute_command(args);
	}
	return 0;
}