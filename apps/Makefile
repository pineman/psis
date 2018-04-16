CC=gcc
#CCX=
CFLAGS=-Wall -Wextra -pedantic
CFLAGS+=-O0
#CFLAGS+=-g -Og
#CFLAGS+=-pg
#CXXFLAGS=$(CFLAGS)
CPPFLAGS=
LDFLAGS=
LDLIBS=
SRCDIR=.
SRC!=find $(SRCDIR) -type f -name "*.c"

all: $(SRC:%.c=%)

%: %.c ../library/libclipboard.o
	$(CC) -c -o $@.o $(CPPFLAGS) $(CFLAGS) $<
	$(CC) -o $@ $(LDFLAGS) $(LDLIBS) $@.o ../library/libclipboard.o

.PHONY: clean
clean:
	rm -f $(SRC:%.c=%) $(SRC:%.c=%.o)
