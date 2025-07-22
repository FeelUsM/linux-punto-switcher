// gcc keyview.c -o keyview -lncurses 
//-I/usr/include/libevdev-1.0  -levdev
#define _GNU_SOURCE
#include <fcntl.h>
#include <ncurses.h>
//#undef KEY_F
//#include <linux/uinput.h>
#include <linux/input.h>
//#include <libevdev/libevdev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_KEYS 512
#define ROWS 7
#define COLS 20

typedef struct {
    const char *label;
    int code; // Linux input event KEY_*
    int row, col;
} Key;

Key keys[MAX_KEYS*2];
int key_count = 0;
int pressed[MAX_KEYS] = {0};
int used[MAX_KEYS] = {0};

void add_key(const char *label, int code, int row, int col) {
    if(label==NULL) return;
    for(int i=0; i<key_count; i++)
        if(keys[i].row==row && keys[i].col==col){
            printf("same positions\n");
            printf("%s %d (%d, %d)\n",keys[i].label,keys[i].code,keys[i].row,keys[i].col);
            printf("%s %d (%d, %d)\n",label,code,row,col);
            exit(1);
        }
    keys[key_count] = (Key){label, code, row, col};
    used[code] = 1;

    key_count++;
}

#define arrlen(arr) (sizeof(arr)/sizeof(arr[0]))

void init_keys() {
    // Ряд: ESC F1-F12
    add_key("ESC", KEY_ESC, 0, 0);
    for (int i = 1; i <= 10; i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "F%d", i);
        add_key(strdup(buf), KEY_F1 + i - 1, 0, i+1);
    }
    add_key("F11", KEY_F11, 0, 12);
    add_key("F12", KEY_F12, 0, 13);

    // Ряд: `123...0
    const char *row1[] = {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Back"};
    int codes1[] = {KEY_GRAVE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6,
                    KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL, KEY_BACKSPACE};
    for (size_t i = 0; i < arrlen(row1); i++) add_key(row1[i], codes1[i], 1, (int)i);

    // QWERTY
    const char *row2[] = {"Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter"};
    int codes2[] = {KEY_TAB, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U,
                    KEY_I, KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_ENTER};
    for (size_t i = 0; i < arrlen(row2); i++) add_key(row2[i], codes2[i], 2, (int)i);

    const char *row3[] = {"Caps", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "\\", "Enter"};
    int codes3[] = {KEY_CAPSLOCK, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K,
                    KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_BACKSLASH, KEY_ENTER};
    for (size_t i = 0; i < arrlen(row3); i++) add_key(row3[i], codes3[i], 3, (int)i);

    const char *row4[] = {"Shift","Shift", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "Shift","Shift"};
    int codes4[] = {KEY_LEFTSHIFT, KEY_LEFTSHIFT, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M,
                    KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT, KEY_RIGHTSHIFT};
    for (size_t i = 0; i < arrlen(row4); i++) add_key(row4[i], codes4[i], 4, (int)i);

    const char *row5[] = {"Ctrl", "Win", "Alt", "SPACE", "SPACE", "SPACE", "SPACE", "SPACE", "SPACE", "SPACE", "Alt", "Win", "Menu", "Ctrl"};
    int codes5[] = {KEY_LEFTCTRL, KEY_LEFTMETA, KEY_LEFTALT, KEY_SPACE, KEY_SPACE, KEY_SPACE, KEY_SPACE, KEY_SPACE, KEY_SPACE, KEY_SPACE, KEY_RIGHTALT, KEY_RIGHTMETA, KEY_COMPOSE, KEY_RIGHTCTRL};
    for (size_t i = 0; i < arrlen(row5); i++) add_key(row5[i], codes5[i], 5, (int)i);

    // arrows & control_keys
    const char *controls[] = {"PrScr", "ScoLk", "Pause", 
                            "Insrt", "Home", "PgUp", 
                            "Delet", "End", "PgDwn", 
                            NULL, NULL, NULL, 
                            NULL, "Up", NULL, 
                            "Left", "Down", "Right"};
    int control_codes[] = {KEY_SYSRQ, KEY_SCROLLLOCK, KEY_PAUSE,
                            KEY_INSERT, KEY_HOME, KEY_PAGEUP, 
                            KEY_DELETE, KEY_END, KEY_PAGEDOWN,
                            0, 0, 0,
                            0, KEY_UP, 0,
                            KEY_LEFT, KEY_DOWN, KEY_RIGHT};
    for (size_t i = 0; i < arrlen(control_codes); i++) add_key(controls[i], control_codes[i], (int)i/3, 14+(int)(i%3));


    // NumPad
    const char *numpad[] = {"NumLk", "/", "*", "-", 
                            "7", "8", "9", "+",
                            "4", "5", "6", "+", 
                            "1", "2", "3", "Enter", 
                            "0", "0", ".", "Enter"};
    int numcodes[] = {KEY_NUMLOCK, KEY_KPSLASH, KEY_KPASTERISK, KEY_KPMINUS,
                      KEY_KP7, KEY_KP8, KEY_KP9, KEY_KPPLUS,
                      KEY_KP4, KEY_KP5, KEY_KP6, KEY_KPPLUS,
                      KEY_KP1, KEY_KP2, KEY_KP3, KEY_KPENTER,
                      KEY_KP0, KEY_KP0, KEY_KPDOT, KEY_KPENTER};

    for (size_t i = 0; i < arrlen(numpad); i++) add_key(numpad[i], numcodes[i], 1 + (int)i / 4, 17 + (int)(i % 4));

    // регистрируем неисползованные коды
    int row=8;
    int col=0;
    for(int i=0; i<MAX_KEYS; i++){
        if(!used[i]){
            char * s = malloc(10) ; // non free
            sprintf(s,"%d",i);
            add_key(s,i,row,col);
            col+=1;
            if(col>=21){
                col=0;
                row+=1;
            }
        }
    }
}

int cadr=0;

void draw_keys(WINDOW *win) {//, const char * name) {
    werase(win);
    box(win, 0, 0);
    //mvwprintw(win, 0, 3, " %s ", name);

    for (int i = 0; i < key_count; i++) {
        int r = keys[i].row + 1;
        int c = keys[i].col * 7 + 2;
        if (pressed[keys[i].code]) wattron(win, A_REVERSE | A_BOLD);
        if(pressed[KEY_LEFTCTRL])
            mvwprintw(win, r, c, "[%-5d]", keys[i].code);
        else
            mvwprintw(win, r, c, "[%-5s]", keys[i].label);
        if (pressed[keys[i].code]) wattroff(win, A_REVERSE | A_BOLD);
    }

    mvwprintw(win, 7, 2, "%d", cadr++);

    wrefresh(win);
}

int main() {
    //printf("%d, %ld, %ld, %ld, %d\n", BUS_USB, UI_SET_EVBIT, UI_SET_KEYBIT, UI_DEV_SETUP, UI_DEV_CREATE);

    init_keys();
    // udevadm info -q all -n /dev/input/eventX | grep KEYBOARD
    int fd = open("/dev/input/event2", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    /*
    struct libevdev *dev = NULL;
    if (libevdev_new_from_fd(fd, &dev) < 0) {
        fprintf(stderr, "Failed to init libevdev\n");
        return 1;
    }
    */

    initscr();
    noecho();
    cbreak();
    curs_set(0);

    WINDOW *win = newwin(32, 151, 1, 1);

    while (1) {
        struct input_event ev;
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) continue;

        //int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

        if (/*rc==0 &&*/ ev.type == EV_KEY && ev.code < MAX_KEYS) {
            pressed[ev.code] = ev.value;  // 1=down, 0=up
            draw_keys(win);//,libevdev_get_name(dev));
        }
    }

    endwin();
    close(fd);
    return 0;
}
