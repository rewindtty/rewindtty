VERSION=0.0.1-dev
CC=gcc
CFLAGS=-Wall -Wextra -std=gnu99 -g
CFLAGS += -DREWINDTTY_VERSION=\"$(VERSION)\"
OBJ=src/main.o src/recorder.o src/replayer.o src/utils.o
OUT=build/rewindtty

all: $(OUT)

$(OUT): $(OBJ)
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf build
	rm -f src/*.o