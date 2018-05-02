CC=clang
#CCX=
CFLAGS=-Wall -Wextra -pedantic -std=c99 -pthread
#CFLAGS+=-O0
CFLAGS+=-g -Og
#CFLAGS+=-pg
#CXXFLAGS=$(CFLAGS)
CPPFLAGS=-I../cb_common
LDFLAGS=-pthread
LDLIBS=
SRCDIR=.
SRC!=find $(SRCDIR) -type f -name "*.c"

# Define executables and their link dependencies here
EXECS = clipboard
clipboard_DEPS = main.c local.c remote.c
clipboard_LIBS =

###############################################################################
# No more user editing is needed below
###############################################################################
OBJDIR=obj

define EXEC_templ =
$(1): $$(addprefix $$(OBJDIR)/,$$($(1)_DEPS:%.c=%.o))
	$(HIDE)$(CC) -o $$@ $$(LDFLAGS) $$(LDLIBS) $$($(1)_LIBS:%=-l%) $$^
endef

$(foreach exec,$(EXECS),$(eval $(call EXEC_templ,$(exec))))

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(HIDE)$(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $<

-include $(SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.d)
$(OBJDIR)/%.d: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	@$(CC) $(CPPFLAGS) -MM -MF $@ $^

.PHONY=clean all
.DEFAULT_GOAL=all
all: $(EXECS)

clean:
	$(HIDE)rm -rf $(OBJDIR) $(EXECS) CLIPBOARD_SOCKET

# This rebuilds everything if the Makefile was modified
# http://stackoverflow.com/questions/3871444/making-all-rules-depend-on-the-makefile-itself/3892826#3892826
-include .dummy
.dummy: Makefile
	@touch $@
	@$(MAKE) -s clean
