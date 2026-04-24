include ../user-config.mk

OUT=edit
FILES=edit.c

$(OUT): $(FILES) $(LIBC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OUT)
