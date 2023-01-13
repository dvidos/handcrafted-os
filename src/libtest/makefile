CC = gcc
AR = ar
# -Werror
CFLAGS = -std=gnu99 -ffreestanding   \
	-Wall -Wextra \
	-Wno-unused-parameter \
	-Wno-error=int-to-pointer-cast  \
	-O2 -g -Iinclude

SOURCE_FILES = $(wildcard framework/*.c)
OBJ_FILES = $(SOURCE_FILES:.c=.o)
DEP_FILES = $(OBJ_FILES:.o=.d)

libtest.a: $(OBJ_FILES)
	$(AR) rcs $@ $(LIBC_OBJ_FILES)

%.o: %.c
	$(CC) -MD -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(OBJ_FILES) $(DEP_FILES)



# include depenedencies files, produced by "gcc -MD"
-include $(DEP_FILES)

