CC = gcc

# -g enables debug
CFLAGS = -I. -g
SOURCES = test.c



all: out


out: $(SOURCES)
	$(CC) -o out $(CFLAGS) $(SOURCES)

