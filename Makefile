all: shell.c
	gcc -Wall -o shell shell.c -lreadline && ./shell

clean:
	rm shell

