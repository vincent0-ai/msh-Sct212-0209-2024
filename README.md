# msh

## Build and run

Run these commands in a Linux terminal with GCC and GNU Make installed:

```sh
make
./msh
```

To remove the generated executable:

```sh
make clean
```

## Feature checklist

- [x] Task 1: Display a prompt, read arbitrary-length lines with `getline()`, and free the line buffer on exit/EOF.

- [x] Task 2: Split on spaces and tabs with `strtok()`, ignore blank lines, and terminate the argument vector with `NULL`.
- [x] Task 3: Run programs with `fork()`, `execvp()`, and `waitpid()`; report failures and continue after an unknown command.
- [x] Task 4: Implement `cd [dir]`, `help`, and `exit`; `cd` without a path uses `$HOME`.
- [] Task 5: Show the previous command's status in the prompt, including `128 + signal` for a command stopped by a signal.
- [] Task 6: Run commands ending in `&` in the background, print their PID, and reap completed children with `waitpid(..., WNOHANG)` in the `SIGCHLD` handler.
- [] Task 7: Redirect external-command stdout using `>`, opening the file in the child with create/truncate flags and mode `0644`.
- [] Task 8: Ignore `SIGINT` in the shell and restore the default action in each child process.

## Design explanation

The shell owns the prompt and the current working directory, so it reads one whole line with `getline()` and splits that line into a dynamically sized array of pointers. The pointers refer to words inside the line buffer; they are not separate copies. A blank line produces no command. After each command the shell releases the pointer array, and when input ends it also frees the line buffer.

For an external command, the shell calls `fork()`. The child is a copy of the shell process. It prepares any requested output redirection with `open()` and `dup2()`, then calls `execvp()` to replace itself with the requested program. If `execvp()` succeeds, that child is now running the requested program; if it fails, the child prints the error and exits. The parent uses `waitpid()` for a foreground child and turns its exit result or terminating signal into the status shown by the next prompt. A background child lets the prompt return immediately. A `SIGCHLD` handler uses `waitpid()` with `WNOHANG` to collect completed children, preventing them from remaining as zombies.

`cd` and `exit` must run in the shell process. If `cd` ran in a child, only that child's working directory would change, and the prompt process would stay in its old directory. If `exit` ran in a child, only the child would exit while the shell continued. `help` is also handled directly by the shell. For Ctrl-C, the shell ignores `SIGINT`; before executing a program, the child restores the default signal action so the foreground program can be interrupted normally.

## Known limitations

The parser intentionally handles simple whitespace-separated arguments. It does not implement quoting, variable expansion, pipes, input redirection, command history, or multiple redirections.

## Sources and AI-tool declaration

I used AI to help me write the tests and this README file.
