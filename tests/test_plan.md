# msh test plan

Run `make` and `./msh` in a Linux terminal. Record the observed output/status in the final column after running each case. The current workspace has a native Windows MinGW compiler but no Linux runtime, so the cases below have not been run here; no result is claimed as a successful test.

| Input / action | Expected behaviour | Actual result |
| --- | --- | --- |
| Blank line | A fresh prompt appears; no error. | Not run here: Linux runtime unavailable. |
| `echo hello world` | Prints `hello world`; next prompt shows `msh[0]>`. | Not run here: Linux runtime unavailable. |
| `ls /nonexistent` | `ls` reports an error; next prompt shows non-zero status. | Not run here: Linux runtime unavailable. |
| `nosuchcommand` | Error message; next prompt shows status 127; shell remains available. | Not run here: Linux runtime unavailable. |
| `cd /tmp` then `pwd` | `pwd` prints `/tmp`. | Not run here: Linux runtime unavailable. |
| `sleep 3 &` | PID is printed and prompt returns immediately; finished child is reaped. | Not run here: Linux runtime unavailable. |
| `echo hi > f.txt` then `cat f.txt` | `f.txt` contains `hi`. | Not run here: Linux runtime unavailable. |
| Run `sleep 100`, then press Ctrl-C | Sleep stops, shell remains; next prompt shows status 130. | Not run here: interactive Linux terminal unavailable. |
| Press Ctrl-D at the prompt | Shell exits cleanly. | Not run here: interactive Linux terminal unavailable. |
| **Own test:** `cd` with no argument, then `pwd` | Shell changes to the path in `$HOME`. | Not run here: Linux runtime unavailable. |
| **Own test:** `false` then inspect prompt | Next prompt shows status 1. | Not run here: Linux runtime unavailable. |
| **Own test:** `printf test > out.txt` then `cat out.txt` | File is created/truncated and contains `test`. | Not run here: Linux runtime unavailable. |
| **Own test:** `sleep 1 &`, wait several seconds at the prompt | Prompt remains usable; child is reaped without needing another command. | Not run here: interactive Linux terminal unavailable. |
