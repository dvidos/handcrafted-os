include ../user-config.mk

OUT=usertests
FILES=usertests.c

$(OUT): $(FILES) $(LIBC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OUT)
