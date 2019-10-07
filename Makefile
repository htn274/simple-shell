all: shell.c
	gcc -Wall -O2 -o shell shell.c exec.c parser.c -lreadline

debug:
	gcc -Wall -g -o shell shell.c exec.c parser.c -lreadline

debug2:
	gcc -Wall -g -o shell shell.c exec.c parser.c -lreadline -fsanitize=address,undefined

static:
	gcc shell.c exec.c parser.c /usr/lib/x86_64-linux-gnu/libreadline.a /usr/lib/x86_64-linux-gnu/libncurses.a /usr/lib/x86_64-linux-gnu/libtermcap.a -O2 -o shell
install:
	chsh -s ${PWD}/shell || ((echo ${PWD}/shell | sudo tee -a /etc/shells) && chsh -s ${PWD}/shell)

clean:
	rm shell

