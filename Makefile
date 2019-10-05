all: shell.c
	gcc -Wall -O2 -o shell shell.c exec.c parser.c -lreadline

debug:
	gcc -Wall -g -o shell shell.c exec.c parser.c -lreadline

debug2:
	gcc -Wall -g -o shell shell.c exec.c parser.c -lreadline -fsanitize=address,undefined

clean:
	rm shell

