VERSION=0.0.8-dev
UPLOAD_URL?=
CC=gcc
CFLAGS=-Wall -Wextra -std=gnu99 -g
CFLAGS += -DREWINDTTY_VERSION=\"$(VERSION)\"
ifneq ($(UPLOAD_URL),)
CFLAGS += -DUPLOAD_URL=\"$(UPLOAD_URL)\"
endif
ifneq ($(PLAYER_URL),)
CFLAGS += -DPLAYER_URL=\"$(PLAYER_URL)\"
endif
CFLAGS += -Ilibs/cjson
LDFLAGS=-lcurl -lutil
OBJ=src/main.o src/recorder.o src/replayer.o src/utils.o src/analyzer.o src/uploader.o libs/cjson/cJSON.o
OUT=build/rewindtty

all: clean $(OUT)

$(OUT): $(OBJ)
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf build
	rm -f src/*.o libs/cjson/*.o