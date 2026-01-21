#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO|ICANON|IEXTEN|ISIG); //no line buffer and no screen
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw);
}


int main() {
    enableRawMode();
    char c;
    while(read(STDIN_FILENO, &c,1) == 1 && c != 'q') {
        if (iscntrl(c)) { //see whether they are control characters
            printf("%d\n", c); //print if it's not
        } else {
            printf("%d ('%c')\r\n",c,c);
        }
    }
    return 0;
}