CFLAGS  = -std=c11 -Wswitch-enum -Wall -Wextra -Wpedantic -DBM_DEBUG_LOG -DBM_DEBUG_STDOUT_LOG -g
LDFLAGS = -lX11 -lGL -lGLX

boomer: main.c
	$(CC) $(CFLAGS) -o boomer main.c $(LDFLAGS)
