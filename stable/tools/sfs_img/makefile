FILES = sfs_img.c functions.c

sfs_img: $(FILES)
	$(CC) -g -D__HOST_SYSTEM__ -o $@ $(FILES)

clean:
	rm -f sfs_img
