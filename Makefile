SRC      = src

INCLUDES = -I$(SRC)/
CFLAGS   = -std=c11 -Wswitch-enum -Wall -Wextra -Wpedantic \
	   -DBM_DEBUG_LOG -DBM_DEBUG_STDOUT_LOG -g
LDFLAGS  = -lX11 -lGL -lGLX -lm

PREFIX   ?= /usr/local
BINDIR   =  $(PREFIX)/bin
MANPATH  := /usr/share/man/man1

TARGET := coomer

.PHONY = all
all: $(TARGET)

MAN := coomer.man.1

$(TARGET): $(SRC)/cm_main.c $(SRC)/cm_shaders.h
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ $(SRC)/cm_main.c $(LDFLAGS)

install: $(TARGET) $(MAN)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 $(TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"
	install -d "$(MANPATH)"
	install -m 755 $(MAN) "$(MANPATH)/$(MAN)"
ifdef SUDO_USER
	chown -R $(SUDO_USER):$(SUDO_USER) .
endif

uninstall:
	$(RM) "$(DESTDIR)$(BINDIR)/$(TARGET)"
clean:
	$(RM) -f $(TARGET)
