SRC      = src

INCLUDES = -I$(SRC)/
CFLAGS   = -std=c11 -Wswitch-enum -Wall -Wextra -Wpedantic \
	   -DBM_DEBUG_LOG -DBM_DEBUG_STDOUT_LOG -g
LDFLAGS  = -lX11 -lGL -lGLX -lm

PREFIX   ?= /usr/local
BINDIR   =  $(PREFIX)/bin

TARGET := coomer

.PHONY = all
all: $(TARGET)

$(TARGET): $(SRC)/cm_main.c $(SRC)/cm_shaders.h
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ $(SRC)/cm_main.c $(LDFLAGS)

install: $(TARGET)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 $(TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"
ifdef SUDO_USER
	chown -R $(SUDO_USER):$(SUDO_USER) .
endif

uninstall:
	$(RM) "$(DESTDIR)$(BINDIR)/$(TARGET)"
clean:
	$(RM) -f $(TARGET)
