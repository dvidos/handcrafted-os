include ../user-config.mk

OUT=echo
FILES=echo.c

$(OUT): $(FILES) $(LIBC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OUT)
