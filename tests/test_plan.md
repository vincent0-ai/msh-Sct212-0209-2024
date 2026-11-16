# msh test plan

Run `make` and `./msh` in a Linux terminal. Record the observed output/status in the final column after running each case.

## Required tests

| Input / action | Expected behaviour | Actual result |
| --- | --- | --- |
| Blank line | A fresh prompt appears; no error. | ✅ Prompt reprints `msh[0]>` with no error output. |
| `echo hello world` | Prints text; status 0 | ✅ Prints `hello world`; next prompt shows `msh[0]>`. |
| `ls /nonexistent` | Error from ls; non-zero status | ✅ `ls: cannot access '/nonexistent': No such file or directory`; next prompt shows `msh[2]>`. |
| `nosuchcommand` | Error message; status 127 | ✅ `nosuchcommand: No such file or directory`; next prompt shows `msh[127]>`. |
| `cd /tmp` then `pwd` | Prints `/tmp` | ✅ `pwd` prints `/tmp`; prompt shows `msh[0]>`. |
| `sleep 3 &` | Prompt returns at once; PID shown | ✅ Prints `[<pid>]` and prompt returns immediately; child is reaped after completion. |
| `echo hi > f.txt` then `cat f.txt` | File contains `hi` | ✅ `cat f.txt` prints `hi`; file created with correct content. |
| Run `sleep 100`, then press Ctrl-C | Sleep stops; shell survives; status 130 | ✅ Sleep is interrupted; shell remains running; next prompt shows `msh[130]>`. |
| Ctrl-D at the prompt | Shell exits cleanly | ✅ Shell exits with code 0; no error output. |

## Own tests

| Input / action | Expected behaviour | Actual result |
| --- | --- | --- |
| `cd` with no argument, then `pwd` | Shell changes to the path in `$HOME`. | ✅ `pwd` prints `/home/vinny` (the value of `$HOME`); prompt shows `msh[0]>`. |
| `false` then inspect prompt | Next prompt shows status 1. | ✅ Next prompt shows `msh[1]>`. |
| `printf test > out.txt` then `cat out.txt` | File is created/truncated and contains `test`. | ✅ `cat out.txt` prints `test`; file created with correct content. |
| `help` | Lists built-in commands. | ✅ Prints usage text listing `cd`, `help`, and `exit`. |
| `sleep 1 &`, wait several seconds at the prompt | Prompt remains usable; child is reaped without needing another command. | ✅ PID printed, prompt returns; background child reaped by SIGCHLD handler. |
