SRC      = src

INCLUDES = -I$(SRC)/
CFLAGS   = -std=c11 -Wswitch-enum -Wall -Wextra -Wpedantic \
	   -DBM_DEBUG_LOG -DBM_DEBUG_STDOUT_LOG -g
LDFLAGS  = -lX11 -lGL -lGLX -lm

PREFIX   ?= /usr/local
BINDIR   =  $(PREFIX)/bin
MANPATH  := /usr/share/man/man1

TARGET := coomer

# when using make a directive must be put in `.PHONY` if its not a file output
# PM PHONY's
.PHONY = all  clean  
# INSTALL PHONY's
.PHONY = install install-doc
# UNINSTALL PHONY's
.PHONY = uninstall-doc uninstall

all: $(TARGET)

# this was not the proper one it was 
#MAN := coomer.1
# when it needs to be 
MAN := coomer.1.gz


$(TARGET): $(SRC)/cm_main.c $(SRC)/cm_shaders.h
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ $(SRC)/cm_main.c $(LDFLAGS)

install: $(TARGET) install-doc
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 $(TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"
ifdef SUDO_USER
	chown -R $(SUDO_USER):$(SUDO_USER) .
endif

install-doc: $(MAN)
	install -d "$(MANPATH)"
	install -m 755 $(MAN) "$(MANPATH)/$(MAN)"

uninstall: uninstall-doc
	$(RM) "$(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall-doc:
	$(RM) "$(MANPATH)/$(MAN)"
clean:
	$(RM) -f $(TARGET)
