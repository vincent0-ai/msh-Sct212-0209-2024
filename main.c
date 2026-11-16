#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

char *read_line(void);
char **parse_line(char *line, size_t *argc);
int launch(char **args);
int builtin_cd(char **args);
int builtin_help(char **args);
int builtin_exit(char **args);

static volatile sig_atomic_t foreground_pid;
static volatile sig_atomic_t foreground_status;
static volatile sig_atomic_t foreground_reaped;

static const char *builtin_names[] = {
    "cd",
    "help",
    "exit"
};

static int (*builtin_functions[])(char **) = {
    builtin_cd,
    builtin_help,
    builtin_exit
};

static size_t builtin_count(void) {
    return sizeof(builtin_names) / sizeof(builtin_names[0]);
}

static void reap_children(int signal_number) {
    int saved_errno = errno;
    int status;
    pid_t pid;

    (void)signal_number;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == (pid_t)foreground_pid) {
            foreground_status = status;
            foreground_reaped = 1;
        }
    }
    errno = saved_errno;
}

static int status_code(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    if (WIFSTOPPED(status)) {
        return 128 + WSTOPSIG(status);
    }
    return 1;
}

void shell_loop(void) {
    int running = 1;
    int last_status = 0;

    while (running) {
        printf("msh[%d]> ", last_status);
        fflush(stdout);

        char *line = read_line();
        size_t argc;
        char **argv = parse_line(line, &argc);

        if (argc > 0) {
            size_t i;
            int handled = 0;

            for (i = 0; i < builtin_count(); i++) {
                if (strcmp(argv[0], builtin_names[i]) == 0) {
                    last_status = builtin_functions[i](argv);
                    running = strcmp(argv[0], "exit") != 0;
                    handled = 1;
                    break;
                }
            }

            if (!handled) {
                last_status = launch(argv);
            }
        }

        free(argv);
        free(line);
    }
}

int launch(char **args) {
    size_t argc = 0;
    size_t redirection_index = (size_t)-1;
    const char *output_path = NULL;
    int background = 0;
    sigset_t blocked_signals;
    sigset_t previous_signals;
    pid_t pid;

    while (args[argc] != NULL) {
        argc++;
    }

    if (argc > 0 && strcmp(args[argc - 1], "&") == 0) {
        background = 1;
        args[--argc] = NULL;
    }

    if (argc == 0) {
        fprintf(stderr, "msh: missing command\n");
        return 2;
    }

    for (size_t i = 0; i < argc; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (redirection_index != (size_t)-1 || i == 0 || i + 2 != argc) {
                fprintf(stderr, "msh: usage: command [args ...] > file [ & ]\n");
                return 2;
            }
            redirection_index = i;
            output_path = args[i + 1];
            args[i] = NULL;
        }
    }

    sigemptyset(&blocked_signals);
    sigaddset(&blocked_signals, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &blocked_signals, &previous_signals) == -1) {
        perror("msh: sigprocmask");
        return 1;
    }

    pid = fork();

    if (pid == 0) {
        struct sigaction default_action;

        if (sigprocmask(SIG_SETMASK, &previous_signals, NULL) == -1) {
            perror("msh: sigprocmask");
            _exit(EXIT_FAILURE);
        }

        memset(&default_action, 0, sizeof(default_action));
        default_action.sa_handler = SIG_DFL;
        sigemptyset(&default_action.sa_mask);
        if (sigaction(SIGINT, &default_action, NULL) == -1) {
            perror("msh: sigaction");
            _exit(EXIT_FAILURE);
        }

        if (output_path != NULL) {
            int output_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (output_fd == -1) {
                perror(output_path);
                _exit(EXIT_FAILURE);
            }
            if (dup2(output_fd, STDOUT_FILENO) == -1) {
                perror("msh: dup2");
                close(output_fd);
                _exit(EXIT_FAILURE);
            }
            if (close(output_fd) == -1) {
                perror("msh: close");
                _exit(EXIT_FAILURE);
            }
        }

        execvp(args[0], args);
        perror(args[0]);
        _exit(EXIT_FAILURE);
    }

    if (pid < 0) {
        perror("fork");
        if (sigprocmask(SIG_SETMASK, &previous_signals, NULL) == -1) {
            perror("msh: sigprocmask");
        }
        return 1;
    }

    if (background) {
        if (sigprocmask(SIG_SETMASK, &previous_signals, NULL) == -1) {
            perror("msh: sigprocmask");
            return 1;
        }
        printf("[%ld]\n", (long)pid);
        fflush(stdout);
        return 0;
    }

    foreground_pid = (sig_atomic_t)pid;
    foreground_reaped = 0;
    if (sigprocmask(SIG_SETMASK, &previous_signals, NULL) == -1) {
        perror("msh: sigprocmask");
        foreground_pid = 0;
        return 1;
    }

    for (;;) {
        int status;
        pid_t result = waitpid(pid, &status, WUNTRACED);

        if (result == pid) {
            foreground_pid = 0;
            return status_code(status);
        }
        if (result == -1 && errno == EINTR) {
            continue;
        }
        if (result == -1 && errno == ECHILD && foreground_reaped) {
            foreground_pid = 0;
            return status_code((int)foreground_status);
        }
        if (result == -1) {
            perror("msh: waitpid");
            foreground_pid = 0;
            return 1;
        }
    }
}

int builtin_cd(char **args) {
    const char *directory = args[1];

    if (directory == NULL) {
        directory = getenv("HOME");
        if (directory == NULL) {
            fprintf(stderr, "msh: cd: HOME is not set\n");
            return 1;
        }
    }

    if (chdir(directory) == -1) {
        perror("msh: cd");
        return 1;
    }

    return 0;
}

int builtin_help(char **args) {
    size_t i;

    (void)args;
    puts("Enter program names and arguments, or use one of the built-ins:");
    for (i = 0; i < builtin_count(); i++) {
        printf("  %s\n", builtin_names[i]);
    }

    return 0;
}

int builtin_exit(char **args) {
    (void)args;
    return 0;
}

char **parse_line(char *line, size_t *argc) {
    size_t capacity = 8;
    char **argv = malloc(capacity * sizeof(*argv));
    char *token;

    if (argv == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    *argc = 0;
    token = strtok(line, " \t\r\n");
    while (token != NULL) {
        if (*argc + 1 >= capacity) {
            capacity *= 2;
            char **resized = realloc(argv, capacity * sizeof(*argv));

            if (resized == NULL) {
                perror("realloc");
                free(argv);
                exit(EXIT_FAILURE);
            }

            argv = resized;
        }

        argv[*argc] = token;
        (*argc)++;
        token = strtok(NULL, " \t\r\n");
    }

    argv[*argc] = NULL;
    return argv;
}

char *read_line(void) {
    size_t bufsize = 32;
    char *line = malloc(bufsize);

    if (line == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            free(line);
            exit(EXIT_SUCCESS);
        } else {
            perror("readline");
            free(line);
            exit(EXIT_FAILURE);
        }
    }

    return line;
}

int main(void) {
    struct sigaction ignore_interrupt;
    struct sigaction child_action;

    memset(&ignore_interrupt, 0, sizeof(ignore_interrupt));
    ignore_interrupt.sa_handler = SIG_IGN;
    sigemptyset(&ignore_interrupt.sa_mask);
    if (sigaction(SIGINT, &ignore_interrupt, NULL) == -1) {
        perror("msh: sigaction SIGINT");
        return EXIT_FAILURE;
    }

    memset(&child_action, 0, sizeof(child_action));
    child_action.sa_handler = reap_children;
    child_action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigemptyset(&child_action.sa_mask);
    if (sigaction(SIGCHLD, &child_action, NULL) == -1) {
        perror("msh: sigaction SIGCHLD");
        return EXIT_FAILURE;
    }

    shell_loop();
    return 0;
}
