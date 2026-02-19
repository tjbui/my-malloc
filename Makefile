CC = gcc

CFLAGS = -Wall -Wextra -O2 -fPIC -Isrc
LDFLAGS = -shared
LIBS = -ldl -pthread

TARGET = libmymalloc.so
SRC = src/myMalloc.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET)