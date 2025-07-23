// sudo apt install libdbus-1-dev
// clang keydeamon.c -o keydeamon $(pkg-config --cflags --libs dbus-1) -Wall -Wextra -Wconversion -Woverflow
// sudo chown root keydeamon
// sudo chmod u+s keydeamon
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <wchar.h>
#include <locale.h>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <linux/uinput.h>
#include <linux/input.h>
#include <dbus/dbus.h>

int device_has_keyboard_keys(const char *devpath, char * name) {
    int fd = open(devpath, O_RDONLY);
    if (fd < 0){
        perror(devpath);
        return 0;  
    } 

    unsigned long ev_bits[(EV_MAX + 7)/8] = {0};
    unsigned long key_bits[(KEY_MAX + 7)/8] = {0};

    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
        perror("ioctl EVIOCGBIT");
        close(fd);
        return 0;
    }

    #define BITS_PER_LONG (sizeof(unsigned long) * 8)
    if (!(ev_bits[EV_KEY / BITS_PER_LONG] & (1UL << (EV_KEY % BITS_PER_LONG)))) {
        close(fd);
        return 0; // не поддерживает EV_KEY
    }

    if(ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits)<0){
        perror("ioctl EVIOCGBIT");
        close(fd);
        return 0;
    }

    /*
     printf("%s:",devpath);
    // каждый бит соответствует поддерживаемой клавише устройства
    for(int i=0; i<4; i++){
        printf("%lx ", key_bits[i]);
    }
    printf("\n");
    */
    
    int low_cnt = 0, high_cnt = 0;
    for(unsigned long i=1; i!=0; i<<=1){
        if(key_bits[0]&i) low_cnt+=1;
    }
    for(unsigned long i=1; i!=0; i<<=1){
        if(key_bits[1]&i) low_cnt+=1;
    }

    for(unsigned long i=1; i!=0; i<<=1){
        if(key_bits[2]&i) high_cnt+=1;
    }
    for(unsigned long i=1; i!=0; i<<=1){
        if(key_bits[3]&i) high_cnt+=1;
    }
    // printf("%d %d\n",low_cnt, high_cnt);
    ioctl(fd, EVIOCGNAME(PATH_MAX), name);
    // printf("Устройство: %s (%s)\n", devpath, name);

    close(fd);
    // на физической клавиатуре поддерживается достаточное количество реальных клавиш (чей key_code < 128)
    // и маленькое количество виртуальных клавиш (чей key_code > 128)
    // на виртуальных клавиатурах (/dev/uinput) зачастую поддерживаются почти все виртуальные клавиши
    return low_cnt>60 && high_cnt<60;
}

int find_keyboard(char * path, char * name){ // возвращает количество найденных клавиатур
    DIR *dir = opendir("/dev/input");
    struct dirent *ent;
    int found = 0;

    while ((ent = readdir(dir))) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;
        char lpath[PATH_MAX];
        char lname[PATH_MAX];
        snprintf(lpath, sizeof(lpath), "/dev/input/%s", ent->d_name);

        if (device_has_keyboard_keys(lpath, lname)) {
            if(found>0) {
                printf("найдено больше одной клавиатуры\n");
                printf("клавиатура %s (%s) игнорируется\n",path, name);
            }
            strcpy(path,lpath);
            strcpy(name,lname);
            found++;
        }
    }
    closedir(dir);
    if(found>1)
        printf("выбрана клавиатура %s (%s)\n",path, name);
    return found;
}

int ev_fd = -1, uinput_fd = -1; // input_fd, output_fd

void cleanup() {
    if (ev_fd >= 0) close(ev_fd);
    if (uinput_fd >= 0) {
        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
    }
}

void setup_uinput() {
    // Это устройство позволяет создавать виртуальные устройства ввода, такие как клавиатура или мышь
    uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd < 0) {
        perror("open uinput");
        exit(1);
    }

    // Говорит ядру Linux: "Буду отправлять события нажатий клавиш (EV_KEY)" через это виртуальное устройство
    ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
    // Устанавливает бит "разрешено использовать эту клавишу" для всех keycode'ов от 0 до 255
    for (int i = 0; i < 256; ++i)
        ioctl(uinput_fd, UI_SET_KEYBIT, i);

    // Структура, описывающая настройки устройства
    struct uinput_setup usetup = {
        // идентификаторы виртуального устройства
        .id = { BUS_USB, // тип "шины", по сути просто для отображения (виртуально говорим, что это USB-клавиатура)
            1, 1, 1 }, // vendor, product, version
        .name = "linux-punto-switcher-virtual-input" // имя устройства, отображается, например, в evtest и xinput
    };

    // Передаёт ядру Linux описание устройства
    ioctl(uinput_fd, UI_DEV_SETUP, &usetup);
    // Просим ядро создать виртуальное устройство с заданными ранее параметрами.
    // После этого в системе появится, например, /dev/input/eventN, которое представляет наше новое "виртуальное устройство клавиатуры".
    ioctl(uinput_fd, UI_DEV_CREATE);

    // Ждём 100 миллисекунд (0.1 сек), чтобы дать ядру время полностью зарегистрировать устройство
    usleep(100000);

    // потом можно будет посмотреть это устройство в evtest или cat /proc/bus/input/devices
    atexit(cleanup);
}

DBusError dbus_err;
DBusConnection* dbus_conn;

void setup_dbus(const char* bus_address){
    uid_t user = getuid();
    uid_t root = geteuid();
    if(0!=setreuid((uid_t)-1, user)){ // временно стать пользователем
        perror("setreuid(-1, user)");
        exit(1);
    }

    dbus_error_init(&dbus_err);

    // Открываем соединение по заданному адресу
    dbus_conn = dbus_connection_open(bus_address, &dbus_err);
    if (dbus_error_is_set(&dbus_err)) {
        fprintf(stderr, "Ошибка при открытии соединения: %s\n", dbus_err.message);
        dbus_error_free(&dbus_err);
        exit(1);
    }

    // Протокол и аутентификация
    if (!dbus_bus_register(dbus_conn, &dbus_err)) {
        fprintf(stderr, "Ошибка при регистрации: %s\n", dbus_err.message);
        dbus_error_free(&dbus_err);
        dbus_connection_unref(dbus_conn);
        exit(1);
    }

    if(0!=setreuid((uid_t)-1, root)){ 
        perror("setreuid(-1, root)");
        exit(1);
    }
}

void emit(int type, int code, int val) {
    struct input_event ie = {
        .type = (unsigned short)type,
        .code = (unsigned short)code,
        .value = val,
        .time = {0, 0}
    };
    write(uinput_fd, &ie, sizeof(ie));
}

#define SHIFT_BIT 0x80000

void send_key(int keycode) {
    int shift = keycode&SHIFT_BIT;
    keycode &= ~SHIFT_BIT;
    if(shift) emit(EV_KEY, KEY_LEFTSHIFT, 1);
    emit(EV_KEY, keycode, 1);
    emit(EV_KEY, keycode, 0);
    if(shift) emit(EV_KEY, KEY_LEFTSHIFT, 0);
    emit(EV_SYN, SYN_REPORT, 0);
    usleep(10);
}

void send_keycomb2(int keycode1, int keycode2) {
    emit(EV_KEY, keycode1, 1);
    emit(EV_KEY, keycode2, 1);
    emit(EV_KEY, keycode2, 0);
    emit(EV_KEY, keycode1, 0);

    emit(EV_SYN, SYN_REPORT, 0);
    usleep(10);
}

// ========== логика pause =========

#define MAX_BUF 4024

static int buffer[MAX_BUF];
static int buf_len = 0;
static int space_mode = 0;

#define FL_LSHIFT 0x1
#define FL_RSHIFT 0x2
#define FL_SHIFT 0x3
#define FL_LCTRL 0x4
#define FL_RCTRL 0x8
#define FL_CTRL 0xC
#define FL_LWIN 0x10
#define FL_RWIN 0x20
#define FL_WIN 0x30
#define FL_LALT 0x40
#define FL_RALT 0x80
#define FL_ALT 0xC0

unsigned char state=0;

// #define arrlen(arr) (sizeof(arr)/sizeof(arr[0]))

int printable[] = {KEY_GRAVE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, 
KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE,
KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, 
KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_DOT, 
KEY_KP7, KEY_KP8, KEY_KP9, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP0,
0 };
int spacelike[] = {KEY_SPACE, 0/*, KEY_ENTER*/};
int is_(int code,int printable[]) {
    for(size_t i=0; printable[i]; i++)
        if(code==printable[i])
            return 1;
    return 0;
}
const char * en_letters[] = {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", 
"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]",
"A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", 
"Z", "X", "C", "V", "B", "N", "M", ",", ".", 
"n7", "n8", "n9", "n4", "n5", "n6", "n1", "n2", "n3", "n0"
};
const char * ru_letters[] = {"ё", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", 
"й", "ц", "у", "к", "е", "н", "г", "ш", "щ", "з", "х", "ъ",
"ф", "ы", "в", "а", "п", "р", "о", "л", "д", "ж", "э", 
"я", "ч", "с", "м", "и", "т", "ь", "б", "ю", 
"n7", "n8", "n9", "n4", "n5", "n6", "n1", "n2", "n3", "n0"
};
const char * space_letters[] = {" "/*, "\\n"*/};

void printbuf(){ // отладочная
    printf("%d :",buf_len);

    for(int i=0; i<buf_len; i++){
        int keycode = buffer[i];
        int shift = keycode&SHIFT_BIT;
        keycode &= ~SHIFT_BIT;
        size_t j=0;
        for(; printable[j]; j++)
            if(keycode==printable[j])
                break;
        if(printable[j]){
            if(shift) printf("+%s",en_letters[j]);
            else printf("%s",en_letters[j]);
        }
        else {
            for(;spacelike[j]; j++)
                if(keycode==spacelike[j])
                    break;
            if(spacelike[j]==0) {
                printf("AT POSITION %u BAD CODE %d\n",i,keycode);
                cleanup();
                exit(1);
            }
            printf("+%s",en_letters[j]);
        }
    }

    printf("\n");
}

// ========== логика alt+pause, shift+pause ================
char * get_clipboard(){ // производящая
    DBusMessage* msg;
    DBusMessage* reply;
    char* result;

    // Создание метода вызова: getClipboardContents
    msg = dbus_message_new_method_call(
        "org.kde.klipper",               // service
        "/klipper",                      // path
        "org.kde.klipper.klipper",       // interface
        "getClipboardContents"          // method
    );
    if (!msg) {
        fprintf(stderr, "Не удалось создать сообщение\n");
        exit(1);
    }

    // Отправка и ожидание ответа
    reply = dbus_connection_send_with_reply_and_block(dbus_conn, msg, -1, &dbus_err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&dbus_err)) {
        fprintf(stderr, "Ошибка вызова метода: %s\n", dbus_err.message);
        dbus_error_free(&dbus_err);
        exit(1);
    }

    // Чтение строки из ответа
    if (!dbus_message_get_args(reply, &dbus_err,
                               DBUS_TYPE_STRING, &result,
                               DBUS_TYPE_INVALID)) {
        fprintf(stderr, "Ошибка чтения ответа: %s\n", dbus_err.message);
        dbus_error_free(&dbus_err);
        dbus_message_unref(reply);
        exit(1);
    }

    char * output = malloc(strlen(result)+5);
    if(output==NULL){
        perror("malloc:");
        exit(1);
    }
    strcpy(output,result);

    dbus_message_unref(reply);

    return output;
}
void set_clipboard(char * text){
    DBusMessage* msg;
    DBusMessageIter args;
    //DBusPendingCall* pending;

    // Создаём метод-вызов setClipboardContents
    msg = dbus_message_new_method_call(
        "org.kde.klipper",              // имя сервиса
        "/klipper",                     // путь
        "org.kde.klipper.klipper",      // интерфейс
        "setClipboardContents"          // метод
    );
    if (!msg) {
        fprintf(stderr, "Не удалось создать сообщение\n");
        exit(1);
    }

    // Добавляем аргументы (строку)
    dbus_message_iter_init_append(msg, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &text)) {
        fprintf(stderr, "Не удалось добавить аргумент\n");
        exit(1);
    }

    // Отправляем и игнорируем ответ
    if (!dbus_connection_send(dbus_conn, msg, NULL)) {
        fprintf(stderr, "Отправка не удалась\n");
        exit(1);
    }

    dbus_connection_flush(dbus_conn);
    dbus_message_unref(msg);
}

char * common_converter(char * s, const wchar_t * layout_ru, const wchar_t * layout_en){ // производящая
    size_t buf_len = mbstowcs(NULL,s,0);
    if(buf_len==(size_t)-1){
        fprintf(stderr, "layout_converter: incorrect unicode sequence\n");
        return NULL;
    }
    wchar_t buf[buf_len+1];
    if(mbstowcs(buf,s,buf_len+1) != buf_len || buf[buf_len]!=0){
        fprintf(stderr, "layout_converter conversion1 error\n");
        exit(1);
    }

    int en_count = 0;
    int ru_count = 0;
    for(size_t i=0; i<buf_len; i++){
        if(wcschr(layout_ru,buf[i])!=NULL)
            ru_count++;
        if(wcschr(layout_en,buf[i])!=NULL)
            en_count++;
    }
    for(size_t i=0; i<buf_len; i++){
        wchar_t * p_ru = wcschr(layout_ru,buf[i]);
        wchar_t * p_en = wcschr(layout_en,buf[i]);
        if(p_ru!=NULL && p_en!=NULL) {
            if(ru_count>en_count) p_en=NULL;
            else if(ru_count<en_count) p_ru=NULL;
            else {p_ru = NULL; p_en=NULL;}
        }
        if(p_ru!=NULL)
            buf[i] = layout_en[ p_ru - layout_ru ];
        if(p_en!=NULL)
            buf[i] = layout_ru[ p_en - layout_en ];
    }

    size_t out_len = wcstombs(NULL,buf,0);
    if(out_len==(size_t)-1){
        fprintf(stderr, "layout_converter: incorrect wide characters\n");
        return NULL;
    }
    char * out = malloc(out_len+1);
    if(wcstombs(out,buf,out_len+1) != out_len || out[out_len]!=0){
        fprintf(stderr, "layout_converter conversion2 error\n");
        exit(1);
    }
    return out;
}

const wchar_t * layout_ru = L"ё1234567890-=Ё!\"№;%:?*()_+йцукенгшщзхъЙЦУКЕНГШЩЗХЪфывапролджэ\\ФЫВАПРОЛДЖЭ/ячсмитьбю.ЯЧСМИТЬБЮ,";
const wchar_t * layout_en = L"`1234567890-=~!@#$%^&*()_+qwertyuiop[]QWERTYUIOP{}asdfghjkl;'\\ASDFGHJKL:\"|zxcvbnm,./ZXCVBNM<>?";
char * layout_converter(char * s){ // производящая
    return common_converter(s, layout_ru, layout_en);
}
const wchar_t * layout_lo = L"qwertyuiopasdfghjklzxcvbnmёйцукенгшщзхъфывапролджэячсмитьбю";
const wchar_t * layout_up = L"QWERTYUIOPASDFGHJKLZXCVBNMЁЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ";
char * shift_converter(char * s){ // производящая
    return common_converter(s, layout_lo, layout_up);
}

void check_convert_strings(){
    if(wcslen(layout_ru)!=wcslen(layout_en)){
        printf("len(layout_ru) = %zu; len(layout_en) = %zu; - mismatch\n",wcslen(layout_ru),wcslen(layout_en));
        exit(1);
    }
    if(wcslen(layout_lo)!=wcslen(layout_up)){
        printf("len(layout_lo) = %zu; len(layout_up) = %zu; - mismatch\n",wcslen(layout_lo),wcslen(layout_up));
        exit(1);
    }
}
void change_layout(){ // здесь раскомментируйте ваш способ изменения раскладки а остальные - какомментируйте
    send_key(KEY_CAPSLOCK);
    // send_keycomb2(KEY_LEFTCTRL, KEY_LEFTSHIFT);
}

void prepr(const char * s){ // отладочная
    printf("\"");
    for(;*s; s++){
        if(((unsigned char)*s)<0x20)
            printf("\\x%x",*s);
        else
            printf("%c",*s);
    }
    printf("\"");
}
void prn() { printf("\n"); } // отладочная

void convert(char * (*converter)(char *)){
    #define dd 50000
    char * backgr = get_clipboard(); // через dbus
  //printf("background:");prepr(backgr);prn();
    usleep(dd);
    send_keycomb2(KEY_LEFTCTRL, KEY_X); // через uinput
  //printf("ctrl+x\n");
    char * selected =0;
    for(int i=0; i<35; i++){
        usleep(10000); // каждые 10 мс
        selected = get_clipboard(); // через dbus
        if(strcmp(selected,backgr)!=0) // проверяем, не поменялся ли clipboard
            break;
        else{
            free(selected);
            selected = 0;
        }
    }
    if(selected==0)
        selected = get_clipboard(); // через dbus

  //printf("selected:");prepr(selected);printf(" -> ");
    char * converted = converter(selected);
    if(converted==NULL){
        if((converted = malloc(strlen(selected)+1))==NULL){
            perror("converted = malloc()");
            exit(1);
        }
        strcpy(converted, selected);
    }
  //printf("converted:");prepr(converted);prn();
    set_clipboard(converted); // через dbus
  //printf("set clipboard\n");
    usleep(dd);
    send_keycomb2(KEY_LEFTCTRL, KEY_V); // через uinput
  //printf("ctrl+v\n");
    usleep(dd);
    set_clipboard(backgr); // через dbus
  //printf("set backgound\n");
    free(backgr);
    free(selected);
    free(converted);
    // todo сделать выделение вконце----
  //printf("----\n");
    if(converter==layout_converter)
        change_layout();
}

int main() {
    setup_uinput();
    // Открываем /dev/input/eventX
    char dev_path[PATH_MAX];
    char dev_name[PATH_MAX] = "Unknown";
    if(find_keyboard(dev_path, dev_name)==0){
        printf("physical keyboards not found\n");
        exit(1);
    }

    ev_fd = open(dev_path, O_RDONLY/*|O_NONBLOCK*/);
    if (ev_fd < 0) {
        perror("open event device");
        return 1;
    }
    char path_buf[200];
    if(0>snprintf(path_buf,200,"unix:path=/run/user/%u/bus",getuid())){
        fprintf(stderr,"very strange error\n");
        exit(1);
    };
    setup_dbus(path_buf);
    setlocale(LC_CTYPE, "");
    check_convert_strings();

    struct input_event ev;
    char * (*converter)(char*) =NULL;

    while (1) {
        ssize_t n = read(ev_fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) continue;

        if (/*rc == 0 && */ev.type == EV_KEY) { // key press
            int code = ev.code;
            int value = ev.value;
            //printf("%d %d %x %p\n",value, code, state, converter);
            if(code == KEY_LEFTSHIFT){
                if(value) state |= FL_LSHIFT;
                else state &= ~FL_LSHIFT;
                if(state==0 && converter) { convert(converter); converter = 0; }
            }
            else if(code == KEY_RIGHTSHIFT){
                if(value) state |= FL_RSHIFT;
                else state &= ~FL_RSHIFT;
                if(state==0 && converter) { convert(converter); converter = 0; }
            }
            else if(code == KEY_LEFTCTRL){
                if(value) state |= FL_LCTRL;
                else state &= ~FL_LCTRL;
                if(state==0 && converter) { convert(converter); converter = 0; }
            }
            else if(code == KEY_RIGHTCTRL){
                if(value) state |= FL_RCTRL;
                else state &= ~FL_RCTRL;
                if(state==0 && converter) { convert(converter); converter = 0; }
            }
            else if(code == KEY_LEFTMETA){
                if(value) state |= FL_LWIN;
                else state &= ~FL_LWIN;
                if(state==0 && converter) { convert(converter); converter = 0; }
            }
            else if(code == KEY_RIGHTMETA){
                if(value) state |= FL_RWIN;
                else state &= ~FL_RWIN;
                if(state==0 && converter) { convert(converter); converter = 0; }
            }
            else if(code == KEY_LEFTALT){
                if(value) state |= FL_LALT;
                else state &= ~FL_LALT;
                if(state==0 && converter) { convert(converter); converter = 0; }
            }
            else if(code == KEY_RIGHTALT){
                if(value) state |= FL_RALT;
                else state &= ~FL_RALT;
                if(state==0 && converter) { convert(converter); converter = 0; }
            }
            else if(is_(code,printable)){
                if(value==1){
                    if(state& ~FL_SHIFT){
                        if(code==KEY_S)
                            ; // ignore ctrl+S, alt+S, win+S, ctrl+alt+S ...
                        else
                            buf_len = 0; // reset
                    }
                    else {
                        if(buf_len>=MAX_BUF)
                            buf_len=0;
                        if(space_mode!=0){
                            buf_len = 0;
                            space_mode = 0;
                        }
                        if(state&FL_SHIFT)
                            buffer[buf_len++] = code|SHIFT_BIT;
                        else
                            buffer[buf_len++] = code;
                    }
                }
            }
            else if(is_(code,spacelike) && state==0){
                if(value==1) {
                    space_mode = 1;
                    if(buf_len>=MAX_BUF)
                        buf_len=0;
                    buffer[buf_len++] = code;
                }
            }
            else if(code == KEY_BACKSPACE && state==0) {
                if(value==1 && buf_len>0){
                    buf_len -=1;
                }
            }
            else if(code == KEY_PAUSE && state==0) {
                if(value==1 && buf_len>0){
                    //printbuf();

                    for (int i = 0; i < buf_len; ++i)
                        send_key(KEY_BACKSPACE);

                    change_layout();

                    for (int i = 0; i < buf_len; ++i) {
                        send_key(buffer[i]);
                    }
                }
            } 
            else if (code == KEY_PAUSE && state&FL_ALT && !(state&!FL_ALT) ) {
                if(value==1){
                    buf_len = 0; // reset
                    space_mode = 0;
                    converter = layout_converter; // convert(converter) будет выполнен когда будут отпущены все кнопки состояния (alt и пр.)
                }
            }
            else if (code == KEY_PAUSE && state&FL_SHIFT && !(state&!FL_SHIFT) ) {
                if(value==1){
                    buf_len = 0; // reset
                    space_mode = 0;
                    converter = shift_converter; // convert(converter) будет выполнен когда будут отпущены все кнопки состояния (alt и пр.)
                }
            }
            else {
                buf_len = 0; // reset
                space_mode = 0;
            }
        }
        usleep(1000);
    }

    return 0;
}
