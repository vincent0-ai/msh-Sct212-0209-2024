#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

char *read_line(void);
char **parse_line(char *line, size_t *argc);
int launch(char **args);
int builtin_cd(char **args);
int builtin_help(char **args);
int builtin_exit(char **args);

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

void shell_loop(void) {
    int running = 1;

    while (running) {
        char *line = read_line();
        size_t argc;
        char **argv = parse_line(line, &argc);

        if (argc > 0) {
            size_t i;
            int handled = 0;

            for (i = 0; i < builtin_count(); i++) {
                if (strcmp(argv[0], builtin_names[i]) == 0) {
                    running = builtin_functions[i](argv);
                    handled = 1;
                    break;
                }
            }

            if (!handled) {
                running = launch(argv);
            }
        }

        free(argv);
        free(line);
    }
}

int launch(char **args) {
    pid_t pid = fork();

    if (pid == 0) {
        execvp(args[0], args);
        perror(args[0]);
        _exit(EXIT_FAILURE);
    }

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    for (;;) {
        int status;
        pid_t result = waitpid(pid, &status, WUNTRACED);

        if (result == pid) {
            if (WIFEXITED(status) || WIFSIGNALED(status) || WIFSTOPPED(status)) {
                break;
            }
        } else if (result == -1 && errno == EINTR) {
            continue;
        } else if (result == -1) {
            perror("waitpid");
            break;
        }
    }

    return 1;
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
    }

    return 1;
}

int builtin_help(char **args) {
    size_t i;

    (void)args;
    puts("Enter program names and arguments, or use one of the built-ins:");
    for (i = 0; i < builtin_count(); i++) {
        printf("  %s\n", builtin_names[i]);
    }

    return 1;
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
    shell_loop();
    return 0;
}
