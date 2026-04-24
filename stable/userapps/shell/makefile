include ../user-config.mk

OUT=shell
FILES=shell.c

$(OUT): $(FILES) $(LIBC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OUT)
