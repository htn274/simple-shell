#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define KEY_RTN 13

int is_raw = 0;
struct termios orig_termios;

void disableRawMode() {
    is_raw = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
void enableRawMode() {
    if (is_raw)
        return;

    is_raw = 1;

    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    //raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


void prompt() {
    
}

char *read_cmd() {
    enableRawMode();

    int l = 0;
    char *s = malloc(1);

    while (1) {
        char c = '\0';

        int count = read(STDIN_FILENO, &c, 1) ;
        if (count < 0) {
            free(s);
            s = NULL;
            break;
        } else if(count > 0) {
            if (iscntrl(c)) {
                printf("%d\r\n", c);
            } else {
                printf("%d ('%c')\r\n", c, c);
            }
            if (c == KEY_RTN) break;

            ++l;
            s = realloc(s, l);
            s[l-1] = c;
        }
    }

    s[l] = '\0';

    disableRawMode();

    return s;
}