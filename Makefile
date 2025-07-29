VERSION=0.0.2-dev
CC=gcc
CFLAGS=-Wall -Wextra -std=gnu99 -g
CFLAGS += -DREWINDTTY_VERSION=\"$(VERSION)\"
CFLAGS += -Ilibs/cjson
OBJ=src/main.o src/recorder.o src/replayer.o src/utils.o libs/cjson/cJSON.o
OUT=build/rewindtty

all: $(OUT)

$(OUT): $(OBJ)
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf build
	rm -f src/*.o libs/cjson/*.o