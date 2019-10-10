FILES = builtin.c job.c shell.c exec.c error.c token.c alias.c parser.c

all: shell.c
	gcc -Wall -O2 -o shell $(FILES) -lreadline

debug:
	gcc -Wall -g -o shell $(FILES) -lreadline

debug2:
	gcc -Wall -g -o shell $(FILES) -lreadline -fsanitize=address,undefined

static:
	gcc $(FILES) /usr/lib/x86_64-linux-gnu/libreadline.a /usr/lib/x86_64-linux-gnu/libncurses.a /usr/lib/x86_64-linux-gnu/libtermcap.a -O2 -g -o shell
install:
	chsh -s ${PWD}/shell || ((echo ${PWD}/shell | sudo tee -a /etc/shells) && chsh -s ${PWD}/shell)

clean:
	rm shell

