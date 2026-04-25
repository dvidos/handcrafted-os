include ../user-config.mk

OUT=usertests
FILES=usertests.c

$(OUT): $(FILES) $(LIBC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OUT)

.PHONY: install
install: $(OUT)
	mkdir -p $(BUILD_DIR)/rootfs/bin
	cp $(OUT) -t $(BUILD_DIR)/rootfs/bin
