#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define INITIAL_ARG_CAPACITY 8

static volatile sig_atomic_t foreground_pid;
static volatile sig_atomic_t foreground_status;
static volatile sig_atomic_t foreground_reaped;

static void handle_sigchld(int signal_number)
{
    int saved_errno = errno;
    int status;
    pid_t pid;

    (void)signal_number;
    for (;;) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            if (pid == (pid_t)foreground_pid) {
                foreground_status = status;
                foreground_reaped = 1;
            }
        } else if (pid == -1 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    errno = saved_errno;
}

static void print_help(void)
{
    puts("Built-ins: cd [dir], help, exit");
    puts("Run external programs with arguments; use command > file to redirect output.");
    puts("Add & at the end of a command to run it in the background.");
}

static int run_builtin(char **argv, int *last_status, int *should_exit)
{
    if (strcmp(argv[0], "cd") == 0) {
        const char *directory = argv[1];

        if (directory == NULL) {
            directory = getenv("HOME");
            if (directory == NULL) {
                fprintf(stderr, "msh: cd: HOME is not set\n");
                *last_status = 1;
                return 1;
            }
        }

        if (chdir(directory) == -1) {
            perror("msh: cd");
            *last_status = 1;
        } else {
            *last_status = 0;
        }
        return 1;
    }

    if (strcmp(argv[0], "help") == 0) {
        print_help();
        *last_status = 0;
        return 1;
    }

    if (strcmp(argv[0], "exit") == 0) {
        *should_exit = 1;
        return 1;
    }

    return 0;
}

static int wait_for_foreground(pid_t child)
{
    int status;
    pid_t result;

    do {
        result = waitpid(child, &status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        if (errno != ECHILD || !foreground_reaped) {
            perror("msh: waitpid");
            foreground_pid = 0;
            return 1;
        }
        status = (int)foreground_status;
    }

    foreground_pid = 0;
    foreground_reaped = 0;

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

static void execute_command(char **argv, int *last_status)
{
    size_t argc = 0;
    size_t i;
    size_t output_index = (size_t)-1;
    int background = 0;
    sigset_t block_chld;
    sigset_t previous_mask;
    char *output_path = NULL;
    pid_t child;

    while (argv[argc] != NULL) {
        argc++;
    }
    if (argc == 0) {
        return;
    }

    if (strcmp(argv[argc - 1], "&") == 0) {
        background = 1;
        argv[--argc] = NULL;
    }
    if (argc == 0) {
        return;
    }

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) {
            if (output_index != (size_t)-1 || i + 1 >= argc || i == 0 || i + 2 != argc) {
                fprintf(stderr, "msh: usage: command [args ...] > file [ & ]\n");
                *last_status = 2;
                return;
            }
            output_index = i;
            output_path = argv[i + 1];
            argv[i] = NULL;
            break;
        }
    }

    if (argv[0] == NULL) {
        fprintf(stderr, "msh: missing command\n");
        *last_status = 2;
        return;
    }

    sigemptyset(&block_chld);
    sigaddset(&block_chld, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &block_chld, &previous_mask) == -1) {
        perror("msh: sigprocmask");
        *last_status = 1;
        return;
    }

    child = fork();
    if (child == -1) {
        perror("msh: fork");
        if (sigprocmask(SIG_SETMASK, &previous_mask, NULL) == -1) {
            perror("msh: sigprocmask");
        }
        *last_status = 1;
        return;
    }

    if (child == 0) {
        struct sigaction default_action;

        if (sigprocmask(SIG_SETMASK, &previous_mask, NULL) == -1) {
            perror("msh: sigprocmask");
            _exit(1);
        }

        memset(&default_action, 0, sizeof(default_action));
        default_action.sa_handler = SIG_DFL;
        sigemptyset(&default_action.sa_mask);
        if (sigaction(SIGINT, &default_action, NULL) == -1) {
            perror("msh: sigaction");
            _exit(1);
        }

        if (output_path != NULL) {
            int output_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (output_fd == -1) {
                perror(output_path);
                _exit(1);
            }
            if (dup2(output_fd, STDOUT_FILENO) == -1) {
                perror("msh: dup2");
                close(output_fd);
                _exit(1);
            }
            if (close(output_fd) == -1) {
                perror("msh: close");
                _exit(1);
            }
        }

        execvp(argv[0], argv);
        {
            int exec_error = errno;
            perror(argv[0]);
            _exit(exec_error == ENOENT ? 127 : 126);
        }
    }

    if (background) {
        if (sigprocmask(SIG_SETMASK, &previous_mask, NULL) == -1) {
            perror("msh: sigprocmask");
        }
        printf("[%ld]\n", (long)child);
        fflush(stdout);
        *last_status = 0;
    } else {
        foreground_pid = (sig_atomic_t)child;
        foreground_reaped = 0;
        if (sigprocmask(SIG_SETMASK, &previous_mask, NULL) == -1) {
            perror("msh: sigprocmask");
            foreground_pid = 0;
            *last_status = 1;
            return;
        }
        *last_status = wait_for_foreground(child);
    }
}

int main(void)
{
    char *line = NULL;
    size_t line_capacity = 0;
    int last_status = 0;
    int should_exit = 0;
    struct sigaction ignore_int;
    struct sigaction child_action;

    memset(&ignore_int, 0, sizeof(ignore_int));
    ignore_int.sa_handler = SIG_IGN;
    sigemptyset(&ignore_int.sa_mask);
    if (sigaction(SIGINT, &ignore_int, NULL) == -1) {
        perror("msh: sigaction SIGINT");
        return EXIT_FAILURE;
    }

    memset(&child_action, 0, sizeof(child_action));
    child_action.sa_handler = handle_sigchld;
    sigemptyset(&child_action.sa_mask);
    child_action.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &child_action, NULL) == -1) {
        perror("msh: sigaction SIGCHLD");
        return EXIT_FAILURE;
    }

    while (!should_exit) {
        char **argv;
        size_t argc = 0;
        size_t argv_capacity = INITIAL_ARG_CAPACITY;
        char *token;
        ssize_t line_length;
        int interrupted = 0;

        printf("msh[%d]> ", last_status);
        fflush(stdout);
        errno = 0;
        line_length = getline(&line, &line_capacity, stdin);
        if (line_length == -1) {
            if (errno == EINTR) {
                clearerr(stdin);
                interrupted = 1;
            } else if (feof(stdin)) {
                break;
            } else {
                perror("msh: getline");
                last_status = 1;
                clearerr(stdin);
                interrupted = 1;
            }
        }
        if (interrupted) {
            continue;
        }

        argv = malloc(argv_capacity * sizeof(*argv));
        if (argv == NULL) {
            perror("msh: malloc");
            last_status = 1;
            break;
        }

        token = strtok(line, " \t\r\n");
        while (token != NULL) {
            if (argc + 1 >= argv_capacity) {
                char **grown_argv;
                argv_capacity *= 2;
                grown_argv = realloc(argv, argv_capacity * sizeof(*argv));
                if (grown_argv == NULL) {
                    perror("msh: realloc");
                    free(argv);
                    free(line);
                    return EXIT_FAILURE;
                }
                argv = grown_argv;
            }
            argv[argc++] = token;
            token = strtok(NULL, " \t\r\n");
        }
        argv[argc] = NULL;

        if (argc > 0) {
            if (!run_builtin(argv, &last_status, &should_exit)) {
                execute_command(argv, &last_status);
            }
        }
        free(argv);
    }

    free(line);
    return EXIT_SUCCESS;
}
