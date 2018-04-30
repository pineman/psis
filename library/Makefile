CC=clang
#CCX=
CFLAGS=-Wall -Wextra -pedantic -pthread
#CFLAGS+=-O0
CFLAGS+=-g -Og
#CFLAGS+=-pg
#CXXFLAGS=$(CFLAGS)
CPPFLAGS=
LDFLAGS=-pthread
LDLIBS=
SRCDIR=.
SRC!=find $(SRCDIR) -type f -name "*.c"

all: $(SRC:%.c=%.o)

.PHONY: clean
clean:
	rm -f $(SRC:%.c=%.o)
