include ../config.mk

# Common macros
#   $@ is the target for multiple targets (separated by space)
#   $? are the dependencies newer than the target
#   $^ are all the dependencies
#
# For implicit rules (e.g. ".c.o:")
#   $< is the target that caused the action
#   $* is the plain filename, common to both extensions

# using customly built cross compiler on path
EMU = qemu-system-i386 -m 2G
CFLAGS = -std=gnu99 -ffreestanding -O3 \
		-I./include  \
		-Wall \
		-Wextra  \
		-Werror   \
		-Wno-unused-parameter \
		-Wno-unused-function \
		-Wno-unused-variable \
		-Wno-unused-but-set-variable \
		-Wno-array-bounds \
		-Wno-pointer-sign

KERNEL_ELF = kernel.elf
KERNEL_BIN = kernel.bin

KERNEL_C_FILES = \
	$(wildcard *.c) \
	$(wildcard utils/*.c) \
	$(wildcard misc/*.c) \
	$(wildcard klib/*.c) \
	$(wildcard memory/*.c) \
	$(wildcard drivers/*.c) \
	$(wildcard multitask/*.c) \
	$(wildcard konsole/*.c) \
	$(wildcard devices/*.c) \
	$(wildcard filesys/*.c) \
	$(wildcard filesys/fat/fat.c) \
	$(wildcard filesys/fat/simple_file_system.c) \
	$(wildcard filesys2/vfs_objects/*.c) \
	$(wildcard filesys2/drivers/skeleton_fs/*.c) \
	$(wildcard processes/*.c)

KERNEL_NASM_FILES1 = \
	$(wildcard misc/*.asm) \
	$(wildcard multitask/*.asm)

# we need start.asm to be first
KERNEL_NASM_FILES_WITHOUT_START = $(filter-out misc/start.asm, $(KERNEL_NASM_FILES1))
KERNEL_NASM_FILES = misc/start.asm $(KERNEL_NASM_FILES_WITHOUT_START)

KERNEL_C_OBJS = $(KERNEL_C_FILES:.c=.o)
KERNEL_ASM_OBJS = $(KERNEL_NASM_FILES:.asm=.o)
KERNEL_OBJ_DEPS = $(KERNEL_C_OBJS:.o=.d)


all: $(KERNEL_BIN)

print:
	@echo ---- kernel binary -----
	@echo $(KERNEL_BIN)
	@echo ---- asm objs -----
	@echo $(KERNEL_ASM_OBJS)
	@echo ---- C objs -----
	@echo $(KERNEL_C_OBJS)
	@echo ---- dependency files -----
	@echo $(KERNEL_OBJ_DEPS)

$(KERNEL_ELF): $(KERNEL_ASM_OBJS) $(KERNEL_C_OBJS) linker.ld 
	$(CROSS_CC) -T linker.ld \
		-Wl,--defsym=KERNEL_LOAD_ADDRESS=$(KERNEL_LOAD_ADDRESS) \
		-Wl,--defsym=KERNEL_STACK_TOP=$(KERNEL_STACK_TOP) \
		-o $@ -ffreestanding -O2 -nostdlib \
		$(KERNEL_ASM_OBJS) \
		$(KERNEL_C_OBJS) \
		-lgcc

$(KERNEL_BIN): $(KERNEL_ELF)
	objcopy -O binary $< $@

%.o: %.c
	@# $(CROSS_CC) -MD -c $< -o $@ $(CFLAGS)
	$(CROSS_CC) -MD -c $< -o $@ $(CFLAGS)

%.o: %.asm
	nasm -f elf -DKERNEL_STACK_TOP=$(KERNEL_STACK_TOP) $< -o $@

%.o: %.S
	$(CROSS_AS) $< -o $@


.PHONY: test
test: tests/run_tests

tests/run_tests: tests/run_tests.c $(SOURCE_FILES)
	gcc -o tests/run_tests \
		-m32 \
		-I./include  \
		tests/run_tests.c $(SOURCE_FILES) \
		-Wno-builtin-declaration-mismatch
	./tests/run_tests


clean:
	rm -f $(KERNEL_BIN) $(KERNEL_ELF) $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS) $(KERNEL_OBJ_DEPS)
	rm -f tests/run_tests


# include depenedencies files, produced by "gcc -MD"
-include $(KERNEL_OBJ_DEPS)
