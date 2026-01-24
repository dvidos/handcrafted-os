

all: standalone32 standalone64 hosted


standalone32:
	gcc -o $@ -DSTANDALONE -m32 --freestanding -nolibc $(wildcard ./src/*.c)

standalone64:
	gcc -o $@ -DSTANDALONE -m64 --freestanding -nolibc  $(wildcard ./src/*.c)

hosted:
	gcc -o $@ -DHOSTED $(wildcard ./src/*.c)

