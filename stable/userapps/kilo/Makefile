include ../user-config.mk

# needed: tcsetattr(), TCSAFLUSH, atexit, tcgetattr(), 
# needed: BRKINT | ICRNL | INPCK | ISTRIP | IXON, OPOST, CS8, ECHO, ICANON, IEXTEN, ISIG, VMIN, VTIME
# needed: sscanf(), struct winsize, getline



all: kilo

kilo: kilo.c $(LIBC)
	$(CC) -o kilo kilo.c -Wall -W $(CFLAGS) $(LDFLAGS)

clean:
	rm kilo
