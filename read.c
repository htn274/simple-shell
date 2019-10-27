#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "read.h"
#include "hist.h"

#define KEY_NUL 0
#define KEY_RTN 13
#define KEY_ESC 27
#define KEY_INT 3
#define KEY_BCK 127

int is_raw = 0;
struct termios orig_termios;

static int cap = 0;
static int len = 0;
static int cur = 0; //cursor position
static char *input = NULL;

#define PATH_MAX 1024

char _read() {
    char c = '\0';
    while (1) {
        if (read(STDIN_FILENO, &c, 1) < 0) {
            c = '\0';
            break;
        }

        if (c != '\0')
            break;

    }

    return c;
}

static inline void _write(char *s, int len) {
    if (write(STDOUT_FILENO, s, len) < 0)
        exit(1);
}

void print_wd() {
    char * wd = getcwd(NULL, PATH_MAX);
    _write("\x1B[38;5;3m", 9);
    _write(wd, strlen(wd));
    _write("\x1B[0m\n", 5);
    free(wd);
}


int get_pos(int *row, int *col) {
    char buf[30]={0};
    int ret, i, pow;
    char ch;

    *row = *col = 0;

    struct termios term, restore;

    tcgetattr(0, &term);
    tcgetattr(0, &restore);
    term.c_lflag &= ~(ICANON|ECHO);
    tcsetattr(0, TCSANOW, &term);

    fflush(stdout);
    _write("\033[6n", 4);

    for( i = 0, ch = 0; ch != 'R'; i++ )
    {
        ret = read(0, &ch, 1);
        if ( !ret ) {
            tcsetattr(0, TCSANOW, &restore);
            return 1;
        }
        buf[i] = ch;
    }

    tcsetattr(0, TCSANOW, &restore);
    if (i < 2)
        return 1;

    for( i -= 2, pow = 1; buf[i] != ';'; i--, pow *= 10)
        *col = *col + ( buf[i] - '0' ) * pow;

    for( i-- , pow = 1; buf[i] != '['; i--, pow *= 10)
        *row = *row + ( buf[i] - '0' ) * pow;

    return 0;
}

void new_line() {
    int row = 0, col = 0;
    if (!get_pos(&row, &col)) {
        if (col > 1)
            _write("\n",1);
    }
}

void disableRawMode() {
    if (is_raw) {
        is_raw = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
}
void enableRawMode() {
    if (is_raw)
        return;

    is_raw = 1;

    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    //raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void print_prompt() {
    print_wd();
    _write(SHELL_NAME, strlen(SHELL_NAME));
    fflush(stdout);
}

void clear_buffer() {
    if (!input)
        input = malloc(1);

    len = cur = 0;
}


void go_left() {
    if (cur > 0) {
        --cur;
        _write("\e[D", 3);
    }
}

void go_right() {
    if (cur < len) {
        ++cur;
        _write("\e[C", 3);
    }
}

void go_front() {
    while (cur > 0)
        go_left();
}


void go_end() {
    while (cur < len)
        go_right();
}

void backspace() {
    if (cur == 0)
        return;
    
    int i;
    for (i = cur; i < len; ++i)
        input[i-1] = input[i];

    --len;
    --cur;

    int count = len - cur + 1;

    _write("\b", 1);
    _write(input + cur, len - cur);
    _write(" ", 1);

    for (i = 0; i  < count; ++i)
        _write("\b", 1);
}

void clear_text() {
    go_end();
    while (len > 0)
        backspace();
}

void insert(char c) {
    ++len;
    if (len + 1 > cap) {
        cap = len + 1;
        input = realloc(input, len + 1);
    }

    int i;
    for (i = len - 1; i > cur; --i)
        input[i] = input[i-1];
    
    input[cur] = c;

    int count = len - cur - 1;
    _write(input + cur, len - cur);

    for (i = 0; i  < count; ++i)
        _write("\b", 1);

    ++cur;
}

void finish() {
    input[len] = '\0';
}

void set_text(const char *s) {
    clear_text();

    if(!s)
        return;

    int i = 0;
    while (s[i]) {
        insert(s[i]);
        ++i;
    }
}

char *read_cmd() {
    enableRawMode();

    clear_buffer();

    int hist_id = hist_len();

    char *cur_input = NULL;

    while (1) {
        char c = _read();

        if (iscntrl(c)) {
            if (c == KEY_NUL) {
                clear_buffer();
                break;
            } else if (c == KEY_ESC) {
                if (!(c = _read())) {
                    clear_buffer();
                    break;
                }

                if (c == '[') {
                    if (!(c = _read())) {
                        clear_buffer();
                        break;
                    }
                    if (c == 'C') {
                        go_right();
                    } else if (c == 'D') {
                        go_left();
                    } else if (c == 'H') {
                        go_front();
                    } else if (c == 'F') {
                        go_end();
                    } else if (c == '3') {
                        if (_read() == '~') {
                            if (cur < len) {
                                go_right();
                                backspace();
                            }
                        }
                    } else if (c == 'A') {
                        if (hist_id <= 0)
                            continue;

                        if (hist_id == hist_len()) {
                            finish();
                            cur_input = strdup(input);
                        }

                        --hist_id;                        
                        set_text(get_history(hist_id));
                    } else if (c == 'B') {
                        if (hist_id >= hist_len())
                            continue;
                        ++hist_id;
                        if (hist_id < hist_len())
                            set_text(get_history(hist_id));
                        else {
                            set_text(cur_input);
                            free(cur_input);
                            cur_input = NULL; 
                        }
                    } else {
                        _write("^[", 2);
                        _write(&c, 1);
                    }
                }
            } else if (c == KEY_INT) {
                clear_buffer();
                _write("\n", 1);
                break;
            } else if (c == KEY_RTN) {
                _write("\n", 1);                
                break;
            } else if (c == KEY_BCK) {
                backspace();
            } else {
                char num[20];
                int l = sprintf(num,"(%d)",c);
                _write(num, l);
            }
        } else {
            insert(c);
        }
    }

    free(cur_input);

    finish();

    disableRawMode();

    return strdup(input);
}