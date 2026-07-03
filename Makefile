CC       ?= cc

GLEW_DIR = vendor/glew-2.3.1/
SRC      = src

INCLUDES = -I$(GLEW_DIR)/include/
CFLAGS   = -std=c11 -Wswitch-enum -Wall -Wextra -Wpedantic \
	   -DBM_DEBUG_LOG -DBM_DEBUG_STDOUT_LOG -g
LDFLAGS  = -lX11 -lGL -lGLX -lm

PREFIX   ?= /usr/local
BINDIR   =  $(PREFIX)/bin

TARGET := coomer

.PHONY = all
all: $(TARGET)

glew.o: $(GLEW_DIR)
	make clean -C $(GLEW_DIR)
	make debug -C $(GLEW_DIR) 
	cp -v $(GLEW_DIR)/tmp/linux/default/static/glew.o $(SRC)

$(TARGET): $(SRC)/main.c $(SRC)/bm_shaders.h glew.o
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ $(SRC)/main.c $(SRC)/glew.o $(LDFLAGS)

install: $(TARGET)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 $(TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"
ifdef SUDO_USER
	chown -R $(SUDO_USER):$(SUDO_USER) .
endif

uninstall:
	$(RM) "$(DESTDIR)$(BINDIR)/$(TARGET)"
clean:
	$(RM) -f $(TARGET) $(SRC)/glew.o
	$(MAKE) -C $(GLEW_DIR) clean
