Simple version of a shell programmed in C.

You can:

- Execute commands with arguments  ⮕  command arg1 arg2...

- Use redirections  ⮕  command args... > file       or    command args... < file

- Redirect to /dev/null  ⮕  command args... &

- Assign variables  ⮕  cmd=ls     to use them use "$"  ⮕   $cmd

Usage: first compile with "gcc -o shell shell.c", then just run it with "./shell".
