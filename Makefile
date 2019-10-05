all: shell.c
	gcc -Wall -O2 -o shell shell.c -lreadline

debug:
	gcc -Wall -g -o shell shell.c -lreadline

debug2:
	gcc -Wall -g -o shell shell.c -lreadline -fsanitize=address,undefined

clean:
	rm shell

