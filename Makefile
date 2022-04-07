CC = gcc
CFLAGS = -I.
SOURCES = test.c



all: out


out: $(SOURCES)
	$(CC) -o out $(CFLAGS) $(SOURCES)

