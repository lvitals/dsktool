CC = gcc

CFLAGS = -Wall -Wextra -std=c99

TARGET = dsktool

SRC = dsktool.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) $(TARGET)

.PHONY: all clean