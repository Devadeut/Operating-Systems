### Inter-process communication using pipes
makefile
```bash
 all: boardgen.c block.c coordinator.c
 gcc -Wall -o block block.c
 gcc -Wall -o coordinator coordinator.c
 run: all
 ./coordinator
 clean:-rm -f block coordinator
```
run
```bash
make run
```

xterm
```bash
 xterm -T "Block 0" -fa Monospace -fs 15 -geometry "17x8+800+300" -bg "#331100" \-e ./block blockno bfdin bfdout rn1fdout rn2fdout cn1fdout cn2fdout
