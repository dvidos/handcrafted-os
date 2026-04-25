include ../user-config.mk
#
# Makefile for sash
#
# The HAVE_GZIP definition adds the -gzip and -gunzip commands.
# The HAVE_EXT2 definition adds the -chattr and -lsattr comamnds.
#

# CFLAGS = -O2 -Wall -Wmissing-prototypes # -DHAVE_EXT2 # -DHAVE_GZIP
# LDFLAGS = -static -s
# LIBS = -lc


OUT = sash
OBJS = sash.o cmds.o cmd_dd.o cmd_ed.o cmd_grep.o cmd_ls.o cmd_tar.o \
	cmd_gzip.o cmd_find.o cmd_file.o cmd_chattr.o cmd_ar.o utils.o


$(OUT): $(OBJS) $(LIBC) sash.h
	$(CC) -o $@ $(OBJS) $(LIBS) $(LDFLAGS)

install: $(OUT)
	mkdir -p $(BUILD_DIR)/rootfs/bin
	cp $(OUT) -t $(BUILD_DIR)/rootfs/bin

clean:
	rm -f $(OBJS) $(OUT)
