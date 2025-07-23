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
make uninstall-user # без sudo
sudo make uninstall-system
```
### системные компоненты:
- для отслеживания клавиатуры используется `linux/input` - требует root-прав
- для эмуляции нажатий на клавиатуру используется `linux/uinput`  - требует root-прав
- для работы с буфером обмена используется `dbus` (который сейчас настроен на работу с KDE) - требует работу от имени пользователя
# todo
- автоопределение рабочего стола
- deb-пакет
- сделать настройки в json или yaml файле
- сделать задержку после смены раскладки (для Gnome)
- из программки keyview сделать создавалку файла настроек
- потестировать автоопределение клавиатуры на устройствах без физической клавиатуры (tablet pc)
# done
- ~~автоопределение клавиатуры~~ 25-07-24
## как сделать deb-пакет с этим файлом
Чтобы собрать `.deb`-пакет из твоего `keydeamon.c` с установкой в `/usr/local/bin/` и SUID root, нужен следующий план:

---

### ✅ Что будет в пакете:

- `/usr/local/bin/keydeamon` — бинарник с `root:root`, `4755`
    
- `keydeamon.service` — **не включается автоматически**, но ставится либо:
    
    - в `~/.config/systemd/user/` через `postinst`
        
    - или в `/usr/share/keydeamon/keydeamon.service` для копирования вручную
        

---

### 📁 Структура проекта для сборки DEB:

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

### 🔧 1. `debian/control`

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

### 🛠️ 2. `debian/rules`

```makefile
#!/usr/bin/make -f

%:
	dh $@
```

(Сделай файл исполняемым: `chmod +x debian/rules`)

---

### 🧩 3. `debian/install`

```
keydeamon /usr/local/bin
```

---

### ⚙️ 4. `debian/postinst`

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

### 🔌 5. `debian/keydeamon.service`

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

### 🧱 6. Makefile (добавим сборку пакета)

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

### 📦 Сборка пакета

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

### 💡 Альтернатива

Если не хочешь вручную писать `debian/`, можно сгенерировать скелет:

```sh
dh_make -s -f ../keydeamon.tar.gz
```

Но вручную — проще для небольших пакетов.

---

Хочешь, я сгенерирую архив со всеми этими файлами?