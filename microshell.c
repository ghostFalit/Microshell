/* Biblioteki, kolory oraz makrosy */
#define _POSIX_C_SOURCE 200809L
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
#include <time.h>

#define PATH_MAX_LEN 1024
#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define FILE_BUF_SIZE 16384
#define HISTORY_MAX 20

#define C_RED       "\033[1;31m"
#define C_GREEN     "\033[1;32m"
#define C_BLUE      "\033[1;34m"
#define C_RESET     "\033[0m"
#define C_CLEAR     "\033[H\033[J"

char history_list[HISTORY_MAX][MAX_CMD_LEN];
int history_count = 0;

/* Funkcja dodania do historii */
void add_to_history(char *cmd)
{
    int i;
    if (strlen(cmd) == 0) return;
    if (history_count < HISTORY_MAX)
    {
        strcpy(history_list[history_count], cmd);
        history_count++;
    }
    else
    {
        for (i = 1; i < HISTORY_MAX; i++)
        {
            strcpy(history_list[i - 1], history_list[i]);
        }
        strcpy(history_list[HISTORY_MAX - 1], cmd);
    }
}

/* Wyświetlenie znaku zachęty oraz bieżącej żcieżki roboczej */
void type_prompt()
{
    char cwd[PATH_MAX_LEN];
    static char *user = NULL;

    if (user == NULL)
    {
        user = getenv("USER");
        if (user == NULL) user = "unknown";
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

/* Funkcja dzielenia wpisanego ciągu na argumenty, obsługa cydzysłowów */
int parse_command(char *input, char **args)
{
    int j = 0;
    int in_quotes = 0;
    char *p = input;
    char *arg_start = NULL;

    while (*p)
    {
        if (*p == '"')
        {
            in_quotes = !in_quotes;
            memmove(p, p + 1, strlen(p));
            if (arg_start == NULL) arg_start = p;
            continue;
        }
        if (!in_quotes && isspace(*p))
        {
            if(arg_start != NULL)
            {
                *p = '\0';
                if (j < MAX_ARGS - 1) args[j++] = arg_start;
                arg_start = NULL;
            }
        }
        else if (arg_start == NULL) arg_start = p;
        p++;
    }
    if (arg_start != NULL && j < MAX_ARGS - 1) args[j++] = arg_start;
    args[j] = NULL;
    return j;
}

/* Funkcja cd: -, ~, errors */
void builtin_cd(char **args)
{
    char *target_path;
    static char prev_dir[PATH_MAX_LEN] = "";
    char current_dir[PATH_MAX_LEN];
    char home_path[PATH_MAX_LEN];

    if (getcwd(current_dir, sizeof(current_dir)) == NULL)
    {
        perror("cd: getcwd error");
        return;
    }

    if (args[1] == NULL)
    {
        target_path = getenv("HOME");
        if (target_path == NULL)
        {
            fprintf(stderr, "cd: HOME variable not set\n");
            return;
        }
    }
    else if (strcmp(args[1], "-") == 0)
    {
        if (strlen(prev_dir) == 0)
        {
            fprintf(stderr, "cd: OLDPWD not set\n");
            return;
        }
        target_path = prev_dir;
        printf("%s\n", target_path);
    }
    else if (args[1][0] == '~')
    {
        char *home = getenv("HOME");
        if (home == NULL)
        {
            fprintf(stderr, "cd: HOME variable not set\n");
            return;
        }
        snprintf(home_path, sizeof(home_path), "%s%s", home, args[1] + 1);
        target_path = home_path;
    }
    else target_path = args[1];
    
    if (chdir(target_path) != 0) perror("cd");
    else strcpy(prev_dir, current_dir);
}

/* Funkcja touch */
void builtin_touch(char **args)
{
    int fd;

    if (args[1] == NULL) {
        fprintf(stderr, "touch: missing file operand\n");
        return;
    }

    fd = open(args[1], O_WRONLY | O_CREAT, 0644);

    if (fd == -1) {
        perror("touch");
        return;
    }

    close(fd);

    if (utime(args[1], NULL) == -1) {
        perror("touch: utime error");
    }
}

/* Funckja stat */
void builtin_stat(char **args)
{
    struct stat file_stat;

    if (args[1] == NULL)
    {
        fprintf(stderr, "stat: missing file operand\n");
        return;
    }

    if (stat(args[1], &file_stat) == -1)
    {
        perror("stat");
        return;
    }

    printf(" File: %s\n", args[1]);
    printf(" Size: %ld\n", (long)file_stat.st_size);
    printf(" Inode: %lu\n", (unsigned long)file_stat.st_ino);
    printf(" Type: %s\n", S_ISDIR(file_stat.st_mode) ? "directory" : "regular file");
    printf(" Access: (%04o)\n", file_stat.st_mode & 0777);
    printf(" Uid: %d    Gid: %d\n", file_stat.st_uid, file_stat.st_gid);
    printf(" Modify: %s", ctime(&file_stat.st_mtime));
}

/* Funkcja history */
void builtin_history()
{
    int i;
    for (i = 0; i < history_count; i++)
    {
        printf("%s\n", history_list[i]);
    }
}

/* Funckja help */
void builtin_help()
{
    printf("\n--- Microshell by Yaroslav Zamorskyi ---\n");
    printf("1) Wbudowany komendy:\n");
    printf("  - cd [path] - zmienić katalog\n");
    printf("  - exit - wyjść z programu\n");
    printf("  - help - wyświetlić ten komunikat\n");
    printf("2) Dodatkowe bajery: login, kolory, CTRL+C, cudzysłów, clear, history\n");
    printf("3) Własne komendy: cp, touch, stat\n\n");
}

/* Funckja clear */
void builtin_clear()
{
    printf("%s", C_CLEAR);  
}

/* Funkcja cp */
void builtin_cp(char **args)
{
    int src_fd, dst_fd;
    ssize_t n_read;
    char buffer[FILE_BUF_SIZE];
    struct stat src_stat, dst_stat;

    if (args[1] == NULL || args[2] == NULL)
    {
        fprintf(stderr, "cp: missing file operand\n");
        return;
    }

    if (stat(args[1], &src_stat) == -1)
    {
        perror("cp: stat error");
        return;
    }

    if (stat(args[2], &dst_stat) == 0)
    {
        if (src_stat.st_dev == dst_stat.st_dev && src_stat.st_ino == dst_stat.st_ino)
        {
            fprintf(stderr, "cp: '%s' and '%s' are the same file\n", args[1], args[2]);
            return;
        }
    }

    src_fd = open(args[1], O_RDONLY);
    if (src_fd == -1)
    {
        perror("cp: source error");
        return;
    }

    dst_fd = open(args[2], O_WRONLY | O_CREAT | O_TRUNC, src_stat.st_mode);
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
            perror("cp: write error");
            close(src_fd);
            close(dst_fd);
            return;
        }
    }

    if (n_read == -1) perror("cp: read error");
    
    close(src_fd);
    close(dst_fd);
}

/* Funkcja procesów potomnych i zewnętrznych programów: fork(), execvp() */
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
    else if (pid < 0) perror("fork failed");
    else
    {
        while (waitpid(pid, &status, 0) == -1)
        {
            if (errno != EINTR)
            {
                perror("waitpid");
                break;
            }
        }
    }
}

/* Wywołanie odpowiednich funkcji */
int execute_command(char **args) 
{
    if (args[0] == NULL) return 1;
    if (strcmp(args[0], "exit") == 0) return 0;
    if (strcmp(args[0], "cd") == 0) { builtin_cd(args); return 1; }
    if (strcmp(args[0], "help") == 0) { builtin_help(); return 1; }
    if (strcmp(args[0], "clear") == 0) { builtin_clear(); return 1; }
    if (strcmp(args[0], "history") == 0) {builtin_history(); return 1;}
    if (strcmp(args[0], "cp") == 0) { builtin_cp(args); return 1; }
    if (strcmp(args[0], "touch") == 0) { builtin_touch(args); return 1; }
    if (strcmp(args[0], "stat") == 0) { builtin_stat(args); return 1; }

    execute_external(args);
    return 1;
}

/* Obsługa CTRL+C */
void sigint_handler(int signum)
{
    (void)signum;
    write(STDOUT_FILENO, "\n", 1);
}

/* Obsługa sygnałów systemowych */
void setup_signals()
{
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

/* Funkcja main */
int main()
{
    char input_buffer[MAX_CMD_LEN];
    char *args[MAX_ARGS];
    int status = 1;

    setup_signals();

    while (status)
    {
        type_prompt();

        if (fgets(input_buffer, MAX_CMD_LEN, stdin) == NULL)
        {
            if (errno == EINTR)
            {
                errno = 0;
                clearerr(stdin);
                continue;
            }
            printf("\n");
            break;
        }

        if (strchr(input_buffer, '\n') == NULL && !feof(stdin))
        {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }

        input_buffer[strcspn(input_buffer, "\n")] = 0;

        if (strlen(input_buffer) == 0) continue;
        add_to_history(input_buffer);
        parse_command(input_buffer, args);
        status = execute_command(args);
    }
    return 0;
}

/* 
CC = gcc
CFLAGS = -Wall -ansi -pedantic
TARGET = microshell

all: $(TARGET)

$(TARGET): microshell.c
    $(CC) $(CFLAGS) -o $(TARGET) microshell.c

clean:
    rm -f $(TARGET)
*/