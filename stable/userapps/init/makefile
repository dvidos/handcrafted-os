include ../user-config.mk

OUT=init
FILES=init.c

$(OUT): $(FILES) $(LIBC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OUT)

.PHONY: install
install: $(OUT)
	mkdir -p $(BUILD_DIR)/rootfs/bin
	cp $(OUT) -t $(BUILD_DIR)/rootfs/bin

