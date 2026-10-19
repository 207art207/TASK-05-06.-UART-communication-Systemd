CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?=

TARGET = mycom
SRC = src/mycom.c
REMOTE ?= art207-rpi5@ajax-rpi5
REMOTE_DIR ?= /home/art207-rpi5/Projects/TASK-05_UART_communication

.PHONY: all clean deploy install

all: $(TARGET)

$(TARGET): $(SRC) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(LDLIBS)

deploy: $(TARGET)
	ssh $(REMOTE) 'mkdir -p "$(REMOTE_DIR)/logs"'
	scp $(TARGET) $(REMOTE):$(REMOTE_DIR)/$(TARGET)

install: $(TARGET) systemd/uart-echo.service
	ssh $(REMOTE) 'mkdir -p "$(REMOTE_DIR)/install"'
	scp $(TARGET) systemd/uart-echo.service $(REMOTE):$(REMOTE_DIR)/install/
	ssh -t $(REMOTE) 'sudo install -m 755 "$(REMOTE_DIR)/install/$(TARGET)" /usr/local/bin/mycom && \
		sudo install -m 644 "$(REMOTE_DIR)/install/uart-echo.service" /etc/systemd/system/uart-echo.service && \
		sudo systemctl daemon-reload && \
		sudo systemctl enable uart-echo.service'

clean:
	$(RM) $(TARGET)
