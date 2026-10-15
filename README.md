# Homework #3: Execution Integrity and Do Process

- Name: Martin Gonzalez
- Email: martingonzalez@u.boisestate.edu
- Class: CS452-002

## Experience

This assignment was really, really tough for me. Although I feel like
the amount of code I had to write wasn't too much thanks to howb the 
skeletal code gave a good starting point, trying to understand and 
debug things that went wrong felt like a much longer process. 

I followed the developement steps given and started with foreground 
commands which wasn't bad, detoured by the fact I needed to fix all the
symlinks to deq. I made tests as I went since they were easy for this 
project. Sequencing was done alongside these, and then I fixed the parser
and tree to handle redirection. This was a large chunk and a lot of back
and forth between command.c. The background commands had me breaking 
everything and checking every file, just to find out it was an error in
how I linked/unlinked nodes in my deq_rem(), I had to use AI to find 
the bug and it told me about a million other bugs while it was at it.
Pipelining and builtin commands went with little issue. 

## How To Use

Requires the shared GNUmakefile in the parent directory. The makefile builds
all the .c files, and requires deq.c, deq.h, and error.h from HW1 deque as 
files or symlinks. I threw my files in, but the submission instructions 
weren't really clear on this. 

To build and run the tests and the actual shell:

```sh
    make clean
    make shell
    Test/run shell  #runs tests
    ./shell         #starts the shell
```

This will compile all files and the tests_balloc.c that supplies main() and
exercises the allocator directly.

To run valgrind:

```sh
    make valgrind
```


## Tests

The tests are all the same format as the ones given. A folder for each, with a 
input file and expected file to check the output. If one fails it prints
<name> failed. 

This is sample output from running Test/run shell:

Test_badargs
Command.c:35: warning: wrong number of arguments to builtin command
Command.c:66: warning: cd: /nonexistent: No such file or directory
Test_badcmd
Command.c:179: warning: nosuchcommand: execvp() failed
Test_bg_builtin
Test_bg_nowait
Test_cd
Test_cd_dash
Test_cd_rel
Test_echo
Test_pipe_basic
Test_pipe_bg
Test_pipe_builtin
Test_pipe_in
Test_pipe_redir
Test_pipe_wc
Test_pipe_yes
Test_pwd
Test_redir_both
Test_redir_builtin
Test_redir_in
Test_redir_missing
/nonexistent: No such file or directory
Test_redir_out
Test_redir_trunc
Test_seq
Test_wait