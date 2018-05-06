CC=clang
#CCX=
CFLAGS=-Wall -Wextra -pedantic -std=c11 -pthread
#CFLAGS+=-O0
CFLAGS+=-g -Og
#CFLAGS+=-pg
CFLAGS+=-fdiagnostics-color=always
#CXXFLAGS=$(CFLAGS)
CPPFLAGS=-I cb_common -I library
LDFLAGS=-pthread
LDLIBS=
SRCDIR=.
SRC!=find $(SRCDIR) -type f -name "*.c" | cut -d"/" -f2-

# Define executables and their link dependencies here
EXECS = clipboard/clipboard apps/test
clipboard/clipboard_DEPS!=find cb_common clipboard -type f -name "*.c"
apps/test_DEPS!=find cb_common library -type f -name "*.c"
apps/test_DEPS+=apps/test.c

###############################################################################
# No more user editing is needed below
###############################################################################
OBJDIR=.obj

define EXEC_templ =
$(1): $$(addprefix $$(OBJDIR)/,$$($(1)_DEPS:%.c=%.o))
	$(CC) -o $$@ $$(LDFLAGS) $$(LDLIBS) $$($(1)_LIBS:%=-l%) $$^
endef

$(foreach exec,$(EXECS),$(eval $(call EXEC_templ,$(exec))))

-include $(addprefix $(OBJDIR)/, $(SRC:.c=.d))
$(OBJDIR)/%.d: %.c
	@mkdir -p `dirname $@`
	@$(CC) $(CPPFLAGS) -MM -MF $@ $^

$(OBJDIR)/%.o: %.c
	$(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $<

.PHONY=clean all
.DEFAULT_GOAL=all
all: $(EXECS)

clean:
	rm -rf $(OBJDIR) $(EXECS)

# This rebuilds everything if the Makefile was modified
# http://stackoverflow.com/questions/3871444/making-all-rules-depend-on-the-makefile-itself/3892826#3892826
-include .dummy
.dummy: Makefile
	@touch $@
	@$(MAKE) -s clean
