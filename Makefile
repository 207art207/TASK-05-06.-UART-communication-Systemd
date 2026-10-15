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

.PHONY: all clean deploy

all: $(TARGET)

$(TARGET): $(SRC) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(LDLIBS)

deploy: $(TARGET)
	ssh $(REMOTE) 'mkdir -p "$(REMOTE_DIR)/logs"'
	scp $(TARGET) $(REMOTE):$(REMOTE_DIR)/$(TARGET)

clean:
	$(RM) $(TARGET)
