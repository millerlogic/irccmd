# Makefile based on Lua 5.2's Makefile

# Settings

PLAT= none

CC= gcc
CFLAGS= -O2 -Wall $(SYSCFLAGS) $(MYCFLAGS)
LDFLAGS= $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS= -lm -llua5.1 $(SYSLIBS) $(MYLIBS)

RM= rm -f

SYSCFLAGS=
SYSLDFLAGS=
SYSLIBS=

MYCFLAGS=
MYLDFLAGS=
MYLIBS=
MYOBJS=

# End of settings

PLATS= generic linux macosx

BASE_O= bit.o fileio.o utf8v.o $(MYOBJS)

IRCCMD_T= irccmd
IRCCMD_O= irccmd.o

ALL_O= $(BASE_O) $(IRCCMD_O)
ALL_T= $(IRCCMD_T)

# Targets
default: $(PLAT)

all: $(ALL_T)

o: $(ALL_O)

$(IRCCMD_T): $(IRCCMD_O) $(BASE_O)
	$(CC) -o $@ $(LDFLAGS) $(IRCCMD_O) $(BASE_O) $(LIBS)

clean:
	$(RM) $(ALL_T) $(ALL_O)

depend:
	@$(CC) $(CFLAGS) -MM *.c

ALL= all

none:
	@echo "Please do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "    $(PLATS)"

generic: $(ALL)

linux:
	$(MAKE) $(ALL) SYSLIBS="-Wl,-E -ldl"

macosx:
	$(MAKE) $(ALL) CC=clang

.PHONY: all $(PLATS) default o clean depend none

# Do not delete
bit.o: bit.c
fileio.o: fileio.c fileio.h
irccmd.o: irccmd.c frandom.h fileio.h utf8v.h
utf8v.o: utf8v.c utf8v.h
