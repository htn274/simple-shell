#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>

#include "read.h"
#include "hist.h"
#include "token.h"

#define KEY_NUL 0
#define KEY_RTN 13
#define KEY_ESC 27
#define KEY_INT 3
#define KEY_BCK 127
#define KEY_TAB 9

int is_raw = 0;
struct termios orig_termios;

static int cap = 0;
static int len = 0;
static int cur = 0; //cursor position
static char *input = NULL;

void init_read() {
    input = malloc(1);
}

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

void finish() {
    input[len] = '\0';
}


void show_text() {
    finish();
    _write(input, len);
    int i;
    for (i = len; i > cur; --i) {
        _write("\b", 1);
    }
}

void print_prompt() {
    print_wd();
    _write(SHELL_NAME, strlen(SHELL_NAME));
    show_text();
    fflush(stdout);
}

void clear_buffer() {
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

//find common prefix of s and t return the length of the prefix
int common_prefix(char *s, char *t) {
    int i = 0;
    while (s[i] && t[i] && s[i] == t[i])
        ++i;
    
    return i;
}

char *find_extension(const char *dir_path, const char *prefix) {
    DIR *d = opendir(dir_path);

    if (!d)
        return NULL;

    char *found = NULL;

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
            continue;

        if (compare_prefix(dir->d_name, prefix) >= 0) {
            if (found)
                //find common prefix
                found[common_prefix(found, dir->d_name)] = '\0';
            else
                found = strdup(dir->d_name);
        }
    }
    closedir(d);

    return found;
}

int compare(const void *a, const void *b) {
    return strcmp(a, b) > 0;
}

char **find_all_extension(const char *dir_path, const char *prefix) {
    DIR *d = opendir(dir_path);

    if (!d)
        return NULL;

    int len = 0;
    char **list = NULL;

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (compare_prefix(dir->d_name, prefix) >= 0) {
            ++len;
            list = realloc(list, (len + 1) * sizeof(list[0]));
            list[len - 1] = strdup(dir->d_name);
        }
    }

    if (!list)
        return list;

    //sort
    list[len] = NULL;
    //qsort(list, len, sizeof(list[0]), compare);
    closedir(d);

    return list;
}

void show_table(char **list) {
    _write("\n", 1);
    if (!list)
        return;
    int i;
    for (i = 0; list[i]; ++i) {
        _write(list[i], strlen(list[i]));
        _write("\t", 1);
    }
    _write("\n", 1);
}

void free_list(char **list) {
    if (!list)
        return;

    int i;
    for (i = 0; list[i]; ++i)
        free(list[i]);

    free(list);
}

void auto_complete(int double_tab) {

    char *s = NULL;
    if (cur > 0 && !isspace(input[cur - 1])) {
        struct ltoken_t ltok;

        char temp = input[cur];
        input[cur] = '\0';
        tokenize(input, &ltok, 1);
        input[cur] = temp;

        if (ltok.n > 0 && ltok.tok[ltok.n-1].type == TOK_ARG) {
            s = ltok.tok[ltok.n-1].val;
            ltok.tok[ltok.n-1].val = 0;
        } 

        free_ltok(&ltok);
    } 

    if (!s) {
        s = malloc(1);
        s[0] = '\0';
    }

    int n = strlen(s);
    int i = n - 1;
    while (i >= 0 && s[i] != '/')
        --i;

    char *dir_path = NULL;

    if (i < 0) {
        dir_path = ".";
    } else if (i == 0) {
        dir_path = "/";
    } else {
        s[i] = '\0';
        dir_path = s;
    }

    char *prefix= s + i + 1;

    if (double_tab) {
        char ** ext = find_all_extension(dir_path, prefix);
        show_table(ext);
        free_list(ext);
        print_prompt();
    } else {
        char *ext = find_extension(dir_path, prefix);
        if (ext) {
            for (i = strlen(prefix); ext[i]; ++i)
                insert(ext[i]);
            free(ext);
        }
    }

    free(s);
}

char *read_cmd() {
    enableRawMode();

    clear_buffer();

    int double_tab = 0;

    int hist_id = hist_len();

    char *cur_input = NULL;

    while (1) {
        char c = _read();

        if (c != KEY_TAB)
            double_tab = 0;

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
            } else if (c == KEY_TAB) {
                auto_complete(double_tab);
                double_tab = 1;
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

    cur_input = strdup(input);
    clear_buffer();

    return cur_input;
}