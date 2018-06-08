CC=clang
#CXX=
CFLAGS=-std=c11
CFLAGS+=-Wall -Wextra -Wpedantic -Wunused-result -Wunreachable-code
#CFLAGS=-Weverything
#CFLAGS+=-O3
CFLAGS+=-g -Og
#CFLAGS+=-pg
CFLAGS+=-DCB_DBG
CFLAGS+=-fdiagnostics-color=always -march=native -pthread -flto
#CXXFLAGS=$(CFLAGS)
CPPFLAGS=-I cb_common -I library -D_POSIX_C_SOURCE="200809L"
LDFLAGS=-pthread -flto
LDLIBS=
SRCDIR=.
SRC=$(shell find $(SRCDIR) -name "*.c" -not -path "./.vscode/*" | cut -d"/" -f2-)

# Define executables and their link dependencies here
EXECS = clipboard/clipboard apps/copy_cont apps/copy apps/paste_cont apps/paste apps/wait_cont apps/wait apps/print_clipboard
clipboard/clipboard_DEPS=$(shell find cb_common clipboard -type f -name "*.c")
APP_DEPS=$(shell find cb_common library -type f -name "*.c")
apps/copy_DEPS=$(APP_DEPS) apps/copy.c
apps/copy_cont_DEPS=$(APP_DEPS) apps/copy_cont.c
apps/paste_DEPS=$(APP_DEPS) apps/paste.c
apps/wait_DEPS=$(APP_DEPS) apps/wait.c
apps/paste_cont_DEPS=$(APP_DEPS) apps/paste_cont.c
apps/wait_cont_DEPS=$(APP_DEPS) apps/wait_cont.c
apps/print_clipboard_DEPS=$(APP_DEPS) apps/print_clipboard.c

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

.PHONY=all clean remake
.DEFAULT_GOAL=all
all: $(EXECS)

clean:
	rm -rf $(OBJDIR) $(EXECS) compile_commands.json

remake:
	$(MAKE) clean
	$(MAKE)

# This rebuilds everything if the Makefile was modified
# http://stackoverflow.com/questions/3871444/making-all-rules-depend-on-the-makefile-itself/3892826#3892826
-include .dummy
.dummy: Makefile
	@touch $@
	@$(MAKE) -s clean
