#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void die(const char *s) {
    perror(s);
    exit(1);
    
}

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        die("tcgetattr");
    };
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK |ISTRIP| IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO|ICANON|IEXTEN|ISIG); //no line buffer and no screen
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw) == -1) {
        die("tcsetattr");
    };
}


int main() {
    enableRawMode();
    while(1) {
        char c = '\0';
        if (read(STDIN_FILENO, &c,1) == -1 && errno != EAGAIN) die("read");
        if (iscntrl(c)) { //see whether they are control characters
            printf("%d\r\n", c); //print if it's not
        } else {
            printf("%d ('%c')\r\n",c,c);  
        }
        if (c == 'q') break;
    }

    return 0;
}