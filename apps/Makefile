CC=clang
#CCX=
CFLAGS=-Wall -Wextra -pedantic -std=c99 -pthread
#CFLAGS+=-O0
CFLAGS+=-g -Og
#CFLAGS+=-pg
#CXXFLAGS=$(CFLAGS)
CPPFLAGS=-I../library/ -I..
LDFLAGS=-pthread
LDLIBS=
SRCDIR=.
SRC!=find $(SRCDIR) -type f -name "*.c"
LIB_OBJ!=find ../library -type f -name "*.o"

all: $(SRC:%.c=%)

%: %.c $(LIB_OBJ)
	$(CC) -c -o $@.o $(CPPFLAGS) $(CFLAGS) $<
	$(CC) -o $@ $(LDFLAGS) $(LDLIBS) $@.o $(LIB_OBJ)

.PHONY: clean
clean:
	rm -f $(SRC:%.c=%) $(SRC:%.c=%.o)
