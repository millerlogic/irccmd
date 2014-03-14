CC=gcc
LUA_INCLUDE=/usr/include/lua5.1
LIBS=-llua5.1 -lm -ldl
CFLAGS=-I"$(LUA_INCLUDE)" $(LIBS) -Wl,-E
BIN=irccmd

all: irccmd

irccmd:
	$(CC) -o "../$(BIN)" *.c $(CFLAGS)

clean:
	rm -rf *o "../$(BIN)"
