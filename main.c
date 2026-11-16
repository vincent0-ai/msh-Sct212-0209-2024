#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

void shell_loop(void) {
    for (;;) {
        char *line = read_line();
        free(line);
    }
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
