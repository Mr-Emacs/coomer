CC       ?= cc

GLEW_DIR = vendor/glew-2.3.1/
SRC      = src

INCLUDES = -I$(GLEW_DIR)/include/
CFLAGS   = -std=c11 -Wswitch-enum -Wall -Wextra -Wpedantic \
	   -DBM_DEBUG_LOG -DBM_DEBUG_STDOUT_LOG -g
LDFLAGS  = -lX11 -lGL -lGLX -lm

PREFIX   ?= /usr/local
BINDIR   =  $(PREFIX)/bin

TARGET := boomer

.PHONY = all
all: boomer

glew.o: $(GLEW_DIR)
	make clean -C $(GLEW_DIR)
	make debug -C $(GLEW_DIR) 
	cp -v $(GLEW_DIR)/tmp/linux/default/static/glew.o $(SRC)

$(TARGET): $(SRC)/main.c glew.o
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ $(SRC)/main.c $(SRC)/glew.o $(LDFLAGS)

install: $(TARGET)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 $(TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall:
	$(RM) "$(DESTDIR)$(BINDIR)/$(TARGET)"
clean:
	$(RM) -f *.o $(TARGET)
	$(MAKE) -C $(GLEW_DIR) clean
