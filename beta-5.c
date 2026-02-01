#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <utime.h>

#define PATH_MAX_LEN 1024
#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define FILE_BUF_SIZE 4096

#define C_RED 		"\033[1;31m"
#define C_GREEN		"\033[1;32m"
#define C_BLUE		"\033[1;34m"
#define C_RESET 	"\033[0m"
#define C_CLEAR     "\033[H\033[J"

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
	int j = 0;
	int in_quotes = 0;
	int len = strlen(input);
	char *arg_start = NULL;

	for (j = 0; j < MAX_ARGS; j++) args[j] = NULL;

	j = 0;

	for (i = 0; i < len; i++)
	{
		if (input[i] == '"')
		{
			in_quotes = !in_quotes;

			if (in_quotes && arg_start == NULL)
			{
				arg_start = &input[i + 1];
			}
			else if (!in_quotes)
			{
				input[i] = '\0';
				if (arg_start != NULL)
				{
					args[j++] = arg_start;
					arg_start = NULL;
				}
			}
			continue;
		}
		if (!in_quotes && isspace(input[i]))
		{
			if (arg_start != NULL)
			{
				input[i] = '\0';
				args[j++] = arg_start;
				arg_start = NULL;
			}
		}
		else if (arg_start == NULL)
		{
			arg_start = &input[i];
		}
	}

	if (arg_start != NULL)
	{
		args[j++] = arg_start;
	}

	args[j] = NULL;
	return j;
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

void builtin_touch(char **args)
{
	int fd;

	if (args[1] == NULL)
	{
		fprintf(stderr, "touch: missing file operand\n");
		return;
	}

	fd = open(args[1], O_WRONLY | O_CREAT | O_APPEND, 0644);

	if (fd == -1)
	{
		perror("touch");
		return;
	}

	close(fd);

	if (utime(args[1], NULL) == -1)
	{
		perror("touch: utime error");
	}
}

void builtin_help()
{
	printf("\n--- Microshell by Author ---\n");
	printf("1) Wbudowany komendy:\n");
	printf("  - cd [path] - zmienić katlog\n");
	printf("  - exit - wyjść z programu\n");
	printf("  - help - wyświetlić ten komunikat\n");
	printf("2) Dodatkowe bajery: login, kolory, CTRL+C, cudzysłów, clear\n");
	printf("3) Własne komendy: cp & touch\n\n");
}

void builtin_clear()
{
	printf("%s", C_CLEAR);	
}

void builtin_cp(char **args)
{
	int src_fd, dst_fd;
	ssize_t n_read;
	char buffer[FILE_BUF_SIZE];

	if (args[1] == NULL || args[2] == NULL)
	{
		fprintf(stderr, "cp: missing file operand\n");
		return;
	}

	src_fd = open(args[1], O_RDONLY);
	if (src_fd == -1)
	{
		perror("cp: source error");
		return;
	}

	dst_fd = open(args[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (dst_fd == -1)
	{
		perror("cp: destination error");
		close(src_fd);
		return;
	}

	while ((n_read = read(src_fd, buffer, sizeof(buffer))) > 0)
	{
		if (write(dst_fd, buffer, n_read) != n_read)
		{
			perror("cd: write error");
			break;
		}
	}

	if (n_read == -1)
	{
		perror("cd: read error");
	}

	close(src_fd);
	close(dst_fd);
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
			perror(args[0]);
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

	if (strcmp(args[0], "clear") == 0)
	{
		builtin_clear();
		return 1;
	}

	if (strcmp(args[0], "cp") == 0)
	{
		builtin_cp(args);
		return 1;
	}

	if (strcmp(args[0], "touch") == 0)
	{
		builtin_touch(args);
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