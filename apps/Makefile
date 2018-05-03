CC=clang
#CCX=
CFLAGS=-Wall -Wextra -pedantic -std=c99 -pthread
#CFLAGS+=-O0
CFLAGS+=-g -Og
#CFLAGS+=-pg
#CXXFLAGS=$(CFLAGS)
CPPFLAGS=-I../library -I../cb_common
LDFLAGS=-pthread
LDLIBS!=find ../library ../cb_common -type f -name "*.o"
SRCDIR=.
SRC!=find $(SRCDIR) -type f -name "*.c"

all: $(SRC:%.c=%)

%: %.c $(LIB_OBJ)
	$(CC) -c -o $@.o $(CPPFLAGS) $(CFLAGS) $<
	$(CC) -o $@ $(LDFLAGS) $(LDLIBS) $@.o

.PHONY: clean
clean:
	rm -f $(SRC:%.c=%) $(SRC:%.c=%.o)
