#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <termios.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <time.h>



// defines
//
#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3

#define CTRL_KEY(k) ((k) & 0x1f)


enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000, 
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

typedef struct erow {
    int size;
    int rsize;
    char *chars;
    char *render;
} erow;


// store the width and height of the terminal
struct editorConfig {
    int cx, cy;
    int rx;
    int rowoff; //keep track of what row of the file the user is currently scrolled to
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    int dirty;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct termios orig_termios;
    
};

struct editorConfig E;


//status checking
void editorSetStatusMessage(const char *fmt, ...);

// terminal
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4); 
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
    
}

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr");
    };
    atexit(disableRawMode);
    
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK |ISTRIP| IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO|ICANON|IEXTEN|ISIG); //no line buffer and no screen
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 100;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    };
}

int editorReadKey() {
    int nread;
    char c;
    //continue reading the byte till it's eof
    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        
        if (seq[0] == '[') {
            if(seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~'){
                    switch(seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'F': return END_KEY;
                    case 'H': return HOME_KEY;
                }
            }
        }
        else if (seq[0] == 'O') {
            switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        
        return '\x1b';
    } else {
        return c;
    }
    
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
    
    while(i < sizeof(buf) -1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1 ) break;
        if (buf[i] == 'R') break;
        i++;
    }     
    
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    
    return 0;
}


//get window size   
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1|| ws.ws_col == 0) { //get window size
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}
/*Row operations*/
int  editorRowCxtoRx(erow *row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) {
        if (row->chars[j] == '\t') 
        rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        rx++;
    }
    return rx;
}


void editorUpdateRow(erow *row) {
    free(row->render);
    row->render = malloc(row->size + 1);
    
    int tabs = 0;
    int j;
    //find the number of tabs per row
    for (j = 0; j < row -> size; ++j) {
        if (row -> chars[j] == '\t') tabs++;
        free(row->render);
        row -> render = malloc(row->size + tabs*(KILO_TAB_STOP - 1) + 1); //render tabs as multiple space charcters
    }
    
    int idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorAppendRow(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(erow)*(E.numrows + 1));
    
    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);
    
    E.numrows++;
    E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
    if (at < 0 || at > row->size) at = row ->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    E.dirty++;
}

/*** editor operations ***/
void editorInsertChar(int c) {
    if (E.cy == E.numrows) {
        editorAppendRow("",0); //append a new row (giving a brand new erow)
    }
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}


void editorRowDelChar(erow *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

void editorDelChar() {
    if (E.cy == E.numrows) return;

    erow *row = &E.row[E.cy];
    if (E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    }
}
/*    file i/o     */
char *editorRowsToString(int *buflen) {
    int totlen = 0;
    int j ;
    for (j = 0; j < E.numrows; ++j) {
        totlen += E.row[j].size + 1; //+1 for '\n
    }
    *buflen = totlen;
    
    char *buf = malloc(totlen); //anchor
    char *p = buf;//write head
    for (j = 0; j < E.numrows; ++j) {
        memcpy( p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        ++p; //next row
    }
    return buf;
}
void editorOpen(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);
    
    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");
    
    char *line = NULL;
    size_t linecap = 0; //size of current line
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
        linelen--;
        editorAppendRow(line, linelen);
    };
    
    
    free(line);
    fclose(fp);
    E.dirty = 0;
}

void editorSave() {
    if (E.filename == NULL) return;
    
    int len;
    char *buf = editorRowsToString(&len);
    
    int fd = open(E.filename, O_RDWR | O_CREAT, 0644); 
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes writteen to disk", len);
                return;
            } 
        }
        close(fd);
    }
    
    free(buf);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** append buffer ***/
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL,0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    
    if (new == NULL) return;
    memcpy(&new[ab ->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab -> b);
}

/*** output  ***/

void editorScroll() {
    E.rx = 0;
    if (E.cy < E.numrows) {
        E.rx = editorRowCxtoRx(&E.row[E.cy], E.cx);
    }
    
    if (E.cy < E.rowoff) { //move up
        E.rowoff = E.cy; //E.rowoff is always at the top
    }
    if (E.cy >= E.screenrows + E.rowoff) { //move down
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx < E.coloff) {
        E.coloff = E.rx;
    }
    if (E.rx >= E.screencols + E.coloff) {
        E.coloff = E.rx - E.screencols + 1;
    }
    
}

void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; ++y) {
        int filerow = y + E.rowoff; //vertical scrolling
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                "Kilo editor -- version %s", KILO_VERSION);
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen)/2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else {
                abAppend(ab, "~", 1);
            }
        } else {
            int len = E.row[filerow].rsize - E.coloff;  //figure out how much strings is left to draw
            if (len < 0) len = 0; //can't draw negative chars
            //truncate if row size is greather than the screen size
            if (len > E.screencols ) len = E.screencols;
            abAppend(ab, &E.row[filerow].render[E.coloff], len);   
        }
        
        abAppend(ab, "\x1b[K", 3); //clears the right side of whatever printed
        abAppend(ab, "\r\n", 2);
        
        
    }
}

void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab, "\x1b[7m", 4); //switches to inverted colors
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", //do not print more than 20 chars
    E.filename ? E.filename: "[No Name]", E.numrows,
    E.dirty ? "(modified)": "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows); //add 1 to since E.cy is 0 indexed 
    //if filename is very very very long
    if (len > E.screencols)  len = E.screencols;
    
    abAppend(ab, status, len);
    
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3); //switches back to normal formatting
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K" , 3); //clears the end of line
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5) 
    abAppend(ab, E.statusmsg, msglen);
    
}

void editorRefreshScreen() {
    
    editorScroll();
    
    struct abuf ab = ABUF_INIT;
    
    abAppend(&ab, "\x1b[?25l", 6); //turn cursor off
    abAppend(&ab, "\x1b[H", 3); //default is row 1 column 1
    
    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1); //string number print formatted
    abAppend(&ab, buf, strlen(buf));//default is row 1 column 1
    
    abAppend(&ab, "\x1b[?25h", 6); //turn cursor on
    //write the buffer's content to standard output
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


/***input ***/
void editorMoveCursor(int key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    switch(key) {
        case ARROW_LEFT:
        if (E.cx != 0){
            E.cx--;
        } else if (E.cy > 0) {
            E.cy--;
            E.cx = E.row[E.cy].size; // size of the row itself
        }
        break;
        case ARROW_RIGHT:
        if (row && E.cx < row ->size){
            E.cx++;
        } else if (row && E.cx == row->size) {
            E.cy ++;
            E.cx = 0; //size of the row 
        }
        break;
        case ARROW_UP: 
        if (E.cy != 0) {
            E.cy--;
        }
        break;
        case ARROW_DOWN:
        if (E.cy < E.numrows) {
            E.cy++;
        } 
        break;
    }
    
    row = (E.cy > E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0; //next row when cy increases
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
    
    
}


void editorProcessKeypress() {
    static int quit_times = KILO_QUIT_TIMES;

    int c = editorReadKey();
    
    switch(c) {
        case '\r':
        /*TODO*/
        break;
        case CTRL_KEY('q'):
        if (E.dirty && quit_times > 0) {
            editorSetStatusMessage("WARNING!!! file has unsaved changes. "
            "Press Ctrl-Q %d more times to quit.", quit_times);
            quit_times--;
            return;
        }
        write(STDOUT_FILENO, "\x1b[2J", 4); 
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
        case CTRL_KEY('s'):
        editorSave();
        break;
        
        case HOME_KEY:
        E.cx = 0;
        break;
        case END_KEY:
        if (E.cy < E.numrows) {
            E.cx = E.row[E.cy].size;
        }
        break;
        
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
        if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
        break;
            
        
        case PAGE_UP:
        case PAGE_DOWN:
        {
            if (c == PAGE_UP) {
                E.cy = E.rowoff;
            } else if (c == PAGE_DOWN) {
                E.cy = E.rowoff + E.screenrows - 1;
                if (E.cy > E.numrows) E.cy = E.numrows;
            }
        }
            break;
            case ARROW_UP:    
            case ARROW_DOWN:
            case ARROW_RIGHT:
            case ARROW_LEFT:
            editorMoveCursor(c);
            break;   
            case CTRL_KEY('l'):
            case '\x1b':
            break;
            default:
            editorInsertChar(c);
            break;
            
        }

        quit_times = KILO_QUIT_TIMES;
    }
    // initiatlisaiton
void initEditor() {
    
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    
    E.screenrows -= 2; //decrements screenrows such that it doesn't try to draw a line of text
    //at the bottom of the screen
}

void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);

    E.statusmsg_time = time(NULL);
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]); //opening and reading file from disk
    }
    //raw mode entrered
    editorSetStatusMessage("HELP: CTRL-Q = quit | Ctrl-Q to quit");
    
    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}