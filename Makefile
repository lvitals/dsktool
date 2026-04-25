CC = gcc
CFLAGS = -Wall -Wextra -std=c99

TARGET = dsktool
SRC = dsktool.c

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

install: $(TARGET)
	mkdir -p $(BINDIR)
	cp $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	$(RM) $(TARGET)

.PHONY: all clean install uninstall