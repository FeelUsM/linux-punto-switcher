#CC=clang
#CFLAGS='-Wall -Wextra -Wconversion -Woverflow -O2'
DESTDIR = /usr/local/bin
CONFDIR = $(HOME)/.config
SYSTEMD_USER_DIR = $(CONFDIR)/systemd/user
UNIT_FILE = $(SYSTEMD_USER_DIR)/linux-punto-switcher.service

.PHONY: all install user-setup clean uninstall uninstall-system uninstall-user

all: linux-punto-switcher # keyview

linux-punto-switcher: keydeamon.c
	$(CC) keydeamon.c -o linux-punto-switcher $(CFLAGS) $(LDFLAGS) $(shell pkg-config --cflags --libs dbus-1) -ludev

keyview: keyview.c
	$(CC) keyview.c -o keyview $(CFLAGS) $(LDFLAGS) -lncurses 


install: linux-punto-switcher
	install -m 4755 -o root -g root linux-punto-switcher $(DESTDIR)/linux-punto-switcher

user-setup:
	install -d $(SYSTEMD_USER_DIR)
	echo "[Unit]"                                     >  $(UNIT_FILE)
	echo "Description=linux-punto-switcher"           >> $(UNIT_FILE)
	echo ""                                           >> $(UNIT_FILE)
	echo "[Service]"                                  >> $(UNIT_FILE)
	echo "ExecStart=$(DESTDIR)/linux-punto-switcher"  >> $(UNIT_FILE)
	echo "Restart=on-failure"                         >> $(UNIT_FILE)
	echo "WorkingDirectory=$(CONFDIR)"                >> $(UNIT_FILE)
	echo "StandardOutput=journal"                     >> $(UNIT_FILE)
	echo "StandardError=journal"                      >> $(UNIT_FILE)
	echo ""                                           >> $(UNIT_FILE)
	echo "[Install]"                                  >> $(UNIT_FILE)
	echo "WantedBy=default.target"                    >> $(UNIT_FILE)
	systemctl --user daemon-reexec
	systemctl --user daemon-reload
	systemctl --user enable --now $(UNIT_FILE)
	install -d $(CONFDIR)
	install linux-punto-config.yaml $(CONFDIR)/linux-punto-config.yaml
	@echo !!!!!!!!!!!
	@echo edit: $(CONFDIR)/linux-punto-config.yaml
	@echo run: systemctl --user restart linux-punto-switcher.service
	@echo !!!!!!!!!!!

clean:
	rm -f linux-punto-switcher keyview

uninstall:
	@echo "Use 'make uninstall-user' and 'sudo make uninstall-system' separately."

uninstall-system:
	rm -f $(DESTDIR)/linux-punto-switcher

uninstall-user:
	systemctl --user disable --now linux-punto-switcher.service || true
	rm -f $(UNIT_FILE)
	rm -f $(CONFDIR)/linux-punto-config.yaml
