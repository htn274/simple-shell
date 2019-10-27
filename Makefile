FILES = builtin.c job.c shell.c exec.c error.c token.c alias.c parser.c read.c hist.c

all: shell.c
	gcc -Wall -O2 -o shell $(FILES)

debug:
	gcc -Wall -g -o shell $(FILES)

debug2:
	gcc -Wall -g -o shell $(FILES) -fsanitize=address,undefined

install:
	chsh -s ${PWD}/shell || ((echo ${PWD}/shell | sudo tee -a /etc/shells) && chsh -s ${PWD}/shell)

clean:
	rm shell

