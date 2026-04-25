include ../user-config.mk

PROGS := true false echo cat

all: $(PROGS)

.PHONY: install
install: $(PROGS)
	mkdir -p $(BUILD_DIR)/rootfs/bin
	for p in $(PROGS); do \
		cp $$p $(BUILD_DIR)/rootfs/bin; \
	done

$(PROGS): %: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(PROGS) *.o
