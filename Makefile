GLEW_DIR = vendor/glew-2.3.1/

INCLUDES = -I$(GLEW_DIR)/include/
CFLAGS   = -std=c11 -Wswitch-enum -Wall -Wextra -Wpedantic -DBM_DEBUG_LOG -DBM_DEBUG_STDOUT_LOG -g
LDFLAGS  = -lX11 -lGL -lGLX -lm

.PHONY = all
all: boomer 

glew.o: $(GLEW_DIR)
	make clean -C $(GLEW_DIR) 
	make debug -C $(GLEW_DIR) 
	cp -v $(GLEW_DIR)/tmp/linux/default/static/glew.o .

boomer: main.c glew.o
	$(CC) $(INCLUDES) $(CFLAGS) -o boomer main.c glew.o $(LDFLAGS)

clean:
	$(RM) -rv *.o boomer
