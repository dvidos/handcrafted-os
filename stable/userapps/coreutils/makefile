include ../../config.inc.mk

ROOTFS := ../../build/rootfs

PROGS := true false echo cat

all: $(PROGS)
	mkdir -p $(ROOTFS)/bin
	for p in $(PROGS); do cp $$p $(ROOTFS)/bin; done

$(PROGS): %: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f $(PROGS) *.o
