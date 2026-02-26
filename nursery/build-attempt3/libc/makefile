include ../config.mk

LIBC=libc.a



all: $(LIBC)


$(LIBC):
	echo "assume building libc"


tests: $(LIBC)
	# compile and run tests, against the library

clean:
	rm -f *.o $(LIBC)

