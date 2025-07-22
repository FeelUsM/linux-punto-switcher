# linux-punto-switcher
punto-switcher for linux in one .c file

реализованные функции в `keydeamon.c`:
- исправление раскладки последнего слова по нажатию `Pause` (эта фича не требует наличия графического интерфейса. Почти полная аналогия [easy-switcher](https://github.com/freemind001/easy-switcher))
- изменение раскладки выделенного текста по `Alt+Pause`
- изменение регистра выделенного текста по `Shift+Pause`

и еще дополнительная программка `keyview.c` - просто посмотреть как нажимаются клавиши клавиатуры, а также их key-коды

### как оно работает:  
- неотступно следит за нажатием и отпусканием всех клавиш клавиатуры
- при нажатии `Pause` 
	- несколько раз нажимает `backspace`, 
	- потом `CapsLock`, 
	- потом нажимает те же клавиши, которыми было набрано это слово.
- при нажатии `Alt+Pause` или `Shift+Pause` 
	- ждёт когда `Alt` и `Shift` будут отпущены
	- запоминает старое содержимое буфера обмена
	- нажимает `Ctrl+X`, ждёт 0.3 сек
	- считывает новое содержимое буфера обмена, конвертирует его
	- устанавливает новое содержимое буфера обмена
	- нажимает `Ctrl+V`
	- восстанавливает исходное содержимое буфера обмена
	- нажимает `CapsLock`

можно настроить способ изменения раскладки клавиатуры в функции `void change_layout()` в `keydeamon.c`

### как собрать и установить
зависимость: `sudo apt install libdbus-1-dev`
```bash
make
sudo make install
make user-setup # без sudo
```
запуск без установки:
```bash
sudo chown root linux-punto-switcher
sudo chmod u+s linux-punto-switcher
./linux-punto-switcher
```
удаление:
```bash
sudo make uninstall-system
make uninstall-user # без sudo
```
### системные компоненты:
- для отслеживания клавиатуры используется `linux/input` - требует root-прав
- для эмуляции нажатий на клавиатуру используется `linux/uinput`  - требует root-прав
- для работы с буфером обмена используется `dbus` (который сейчас настроен на работу с KDE) - требует работу от имени пользователя
### todo
- автоопределение клавиатуры
- настройка способа переключения раскладки
- автоопределение рабочего стола
- deb-пакет
- русская раскладка с # и numpad-точкой
# как в программе на си без использования библиотек определить к какому файлу /dev/inut/eventX подключена клавиатура
Чтобы **в программе на C определить, к какому `/dev/input/eventX` подключена клавиатура**, **без использования сторонних библиотек** (например, `libevdev`), можно **разобрать содержимое `/proc/bus/input/devices`**, которое предоставляет информацию обо всех устройствах ввода.

---

## 📌 Общая идея

1. Прочитать файл `/proc/bus/input/devices`.
    
2. Найти строки `N: Name="..."`, чтобы определить, что это клавиатура.
    
3. Взять соответствующую строку `H: Handlers=... eventX`.
    
4. Построить путь `/dev/input/eventX`.
    

---

## 📄 Пример: фрагмент `/proc/bus/input/devices`

```
I: Bus=0011 Vendor=0001 Product=0001 Version=ab41
N: Name="AT Translated Set 2 keyboard"
P: Phys=isa0060/serio0/input0
S: Sysfs=/devices/platform/i8042/serio0/input/input3
U: Uniq=
H: Handlers=sysrq kbd event3 leds
B: PROP=0
B: EV=120013
B: KEY=402000000 3803078f800d001 feffffdfffefffff fffffffffffffffe
```

Тут видно, что `"AT Translated Set 2 keyboard"` соответствует `event3`.

---

## ✅ Реализация на C (без libevdev)

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE 512

// Простая проверка: имя содержит одно из ключевых слов
int is_keyboard(const char *name) {
    return strstr(name, "keyboard") || strstr(name, "Keyboard") || strstr(name, "AT Translated");
}

// Находит путь к первому подходящему /dev/input/eventX
char *find_keyboard_device() {
    FILE *fp = fopen("/proc/bus/input/devices", "r");
    if (!fp) {
        perror("fopen");
        return NULL;
    }

    static char event_path[64];
    char line[MAX_LINE];
    char name_line[MAX_LINE] = {0};
    char handlers_line[MAX_LINE] = {0};

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == 'N' && strstr(line, "Name=")) {
            strncpy(name_line, line, sizeof(name_line));
        } else if (line[0] == 'H' && strstr(line, "Handlers=")) {
            strncpy(handlers_line, line, sizeof(handlers_line));
        } else if (line[0] == '\n') {
            // Новый блок — анализируем собранное
            if (is_keyboard(name_line)) {
                // Найдём eventX в строке Handlers
                char *p = strstr(handlers_line, "event");
                if (p) {
                    char event_name[16];
                    sscanf(p, "%15s", event_name);
                    snprintf(event_path, sizeof(event_path), "/dev/input/%s", event_name);
                    fclose(fp);
                    return event_path;
                }
            }
            name_line[0] = '\0';
            handlers_line[0] = '\0';
        }
    }

    fclose(fp);
    return NULL;
}
```

---

## 🔧 Использование

В `main()`:

```c
int main() {
    const char *path = find_keyboard_device();
    if (!path) {
        fprintf(stderr, "Клавиатура не найдена\n");
        return 1;
    }

    printf("Найдено устройство: %s\n", path);
    // открываем path как /dev/input/eventX
}
```

---

## ✅ Преимущества

- Работает **без libevdev** и без других внешних библиотек.
    
- Использует только POSIX и системные интерфейсы `/proc`.
    

---

## ⚠️ Замечания

- Если в системе несколько клавиатур, выберется **первая подходящая**.
    
- Нужны **права root**, чтобы читать `/dev/input/eventX`.
    
- Некоторые устройства могут называться не "keyboard", а "input device" — можно расширить список ключевых слов.
    

---

Хочешь, я встрою это в `keydaemon.c` вместо `libevdev`?


# как сделать deb-пакет с этим файлом
Чтобы собрать `.deb`-пакет из твоего `keydeamon.c` с установкой в `/usr/local/bin/` и SUID root, нужен следующий план:

---

## ✅ Что будет в пакете:

- `/usr/local/bin/keydeamon` — бинарник с `root:root`, `4755`
    
- `keydeamon.service` — **не включается автоматически**, но ставится либо:
    
    - в `~/.config/systemd/user/` через `postinst`
        
    - или в `/usr/share/keydeamon/keydeamon.service` для копирования вручную
        

---

## 📁 Структура проекта для сборки DEB:

```
keydeamon/
├── keydeamon.c
├── Makefile
├── debian/
│   ├── control
│   ├── rules
│   ├── postinst
│   ├── keydeamon.service
│   └── install
```

---

## 🔧 1. `debian/control`

```debcontrol
Source: keydeamon
Section: utils
Priority: optional
Maintainer: Your Name <you@example.com>
Standards-Version: 4.5.0
Package: keydeamon
Architecture: amd64
Depends: ${shlibs:Depends}, ${misc:Depends}
Description: Simple SUID key daemon
 A privileged key monitor with D-Bus access.
```

---

## 🛠️ 2. `debian/rules`

```makefile
#!/usr/bin/make -f

%:
	dh $@
```

(Сделай файл исполняемым: `chmod +x debian/rules`)

---

## 🧩 3. `debian/install`

```
keydeamon /usr/local/bin
```

---

## ⚙️ 4. `debian/postinst`

```sh
#!/bin/sh
set -e

# Установить владельца и права
chown root:root /usr/local/bin/keydeamon
chmod 4755 /usr/local/bin/keydeamon

# Предложить пользователю скопировать unit-файл в ~/.config/systemd/user/
echo "To enable the keydeamon user service, run:"
echo "  mkdir -p ~/.config/systemd/user"
echo "  cp /usr/share/keydeamon/keydeamon.service ~/.config/systemd/user/"
echo "  systemctl --user daemon-reload && systemctl --user enable --now keydeamon.service"
```

(Сделай файл исполняемым: `chmod +x debian/postinst`)

---

## 🔌 5. `debian/keydeamon.service`

```ini
[Unit]
Description=Keydeamon

[Service]
ExecStart=/usr/local/bin/keydeamon
Restart=on-failure

[Install]
WantedBy=default.target
```

---

## 🧱 6. Makefile (добавим сборку пакета)

```makefile
CC = clang
SRC = keydeamon.c
BIN = keydeamon
CFLAGS = -Wall -Wextra -Wconversion -Woverflow $(shell pkg-config --cflags dbus-1)
LDFLAGS = $(shell pkg-config --libs dbus-1)

.PHONY: all clean install deb

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(SRC) -o $(BIN) $(CFLAGS) $(LDFLAGS)

install: $(BIN)
	install -o root -g root -m 4755 $(BIN) /usr/local/bin/$(BIN)

clean:
	rm -f $(BIN)
	dh_clean

deb: clean
	dpkg-buildpackage -us -uc
```

---

## 📦 Сборка пакета

1. Установи нужные инструменты:
    

```sh
sudo apt install build-essential devscripts debhelper dh-make
```

2. Собери пакет:
    

```sh
make deb
```

Появятся файлы:

```
../keydeamon_1.0_amd64.deb
```

3. Установи пакет:
    

```sh
sudo dpkg -i ../keydeamon_1.0_amd64.deb
```

---

## 💡 Альтернатива

Если не хочешь вручную писать `debian/`, можно сгенерировать скелет:

```sh
dh_make -s -f ../keydeamon.tar.gz
```

Но вручную — проще для небольших пакетов.

---

Хочешь, я сгенерирую архив со всеми этими файлами?