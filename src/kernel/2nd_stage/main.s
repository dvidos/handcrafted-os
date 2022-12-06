	.file	"main.c"
	.code16gcc
	.text
	.globl	boot_drive
	.section	.bss
	.type	boot_drive, @object
	.size	boot_drive, 1
boot_drive:
	.zero	1
	.globl	buffer
	.align 32
	.type	buffer, @object
	.size	buffer, 512
buffer:
	.zero	512
	.globl	numbuff
	.align 32
	.type	numbuff, @object
	.size	numbuff, 32
numbuff:
	.zero	32
	.section	.rodata
	.align 4
.LC0:
	.string	"\r\nSecond stage boot loader running...\r\n"
.LC1:
	.string	"Low memory value: "
.LC2:
	.string	" KB\r\n"
.LC3:
	.string	"Error getting memory map\r\n"
.LC4:
	.string	"Memory map follows:\r\n"
.LC5:
	.string	"Type 0x"
.LC6:
	.string	"\r\n"
.LC7:
	.string	"Done\r\n"
	.text
	.globl	start
	.type	start, @function
start:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$24, %esp
/APP
/  40 "main.c" 1
	mov %al, boot_drive
/  0 "" 2
/NO_APP
	subl	$12, %esp
	pushl	$.LC0
	call	bios_print_str
	addl	$16, %esp
	call	bios_get_low_memory_in_kb
	movw	%ax, -18(%ebp)
	movzwl	-18(%ebp), %eax
	subl	$4, %esp
	pushl	$10
	pushl	$numbuff
	pushl	%eax
	call	itoa
	addl	$16, %esp
	subl	$12, %esp
	pushl	$.LC1
	call	bios_print_str
	addl	$16, %esp
	subl	$12, %esp
	pushl	$numbuff
	call	bios_print_str
	addl	$16, %esp
	subl	$12, %esp
	pushl	$.LC2
	call	bios_print_str
	addl	$16, %esp
	subl	$4, %esp
	pushl	$512
	pushl	$0
	pushl	$buffer
	call	memset
	addl	$16, %esp
	subl	$12, %esp
	pushl	$buffer
	call	bios_detect_memory_map
	addl	$16, %esp
	movl	%eax, -24(%ebp)
	cmpl	$0, -24(%ebp)
	je	.L2
	subl	$12, %esp
	pushl	$.LC3
	call	bios_print_str
	addl	$16, %esp
	jmp	.L6
.L2:
	subl	$12, %esp
	pushl	$.LC4
	call	bios_print_str
	addl	$16, %esp
	movl	$buffer, -12(%ebp)
	movl	$0, -16(%ebp)
	jmp	.L4
.L5:
	subl	$4, %esp
	pushl	$32
	pushl	$0
	pushl	$numbuff
	call	memset
	addl	$16, %esp
	movl	-12(%ebp), %eax
	movl	16(%eax), %eax
	movzwl	%ax, %eax
	subl	$4, %esp
	pushl	$16
	pushl	$numbuff
	pushl	%eax
	call	itoa
	addl	$16, %esp
	subl	$12, %esp
	pushl	$.LC5
	call	bios_print_str
	addl	$16, %esp
	subl	$12, %esp
	pushl	$numbuff
	call	bios_print_str
	addl	$16, %esp
	subl	$12, %esp
	pushl	$.LC6
	call	bios_print_str
	addl	$16, %esp
	addl	$24, -12(%ebp)
	addl	$1, -16(%ebp)
.L4:
	cmpl	$7, -16(%ebp)
	jle	.L5
	subl	$12, %esp
	pushl	$.LC7
	call	bios_print_str
	addl	$16, %esp
.L6:
/APP
/  115 "main.c" 1
	hlt
/  0 "" 2
/NO_APP
	jmp	.L6
	.size	start, .-start
	.globl	bios_print_char
	.type	bios_print_char, @function
bios_print_char:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$4, %esp
	movl	8(%ebp), %eax
	movb	%al, -4(%ebp)
/APP
/  123 "main.c" 1
	mov $0x0e, %ah 
	mov -4(%ebp), %al 
	int $0x10 
	
/  0 "" 2
/NO_APP
	nop
	leave
	ret
	.size	bios_print_char, .-bios_print_char
	.globl	bios_print_str
	.type	bios_print_str, @function
bios_print_str:
	pushl	%ebp
	movl	%esp, %ebp
	jmp	.L9
.L10:
	movl	8(%ebp), %eax
	movzbl	(%eax), %eax
	movzbl	%al, %eax
	pushl	%eax
	call	bios_print_char
	addl	$4, %esp
	addl	$1, 8(%ebp)
.L9:
	movl	8(%ebp), %eax
	movzbl	(%eax), %eax
	testb	%al, %al
	jne	.L10
	nop
	nop
	leave
	ret
	.size	bios_print_str, .-bios_print_str
	.globl	bios_get_low_memory_in_kb
	.type	bios_get_low_memory_in_kb, @function
bios_get_low_memory_in_kb:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$16, %esp
	movw	$0, -2(%ebp)
	movw	$0, -4(%ebp)
/APP
/  145 "main.c" 1
	clc            
	mov $0, %dx 
	int $0x12      
	jnc 1f    
	mov $1, %dx     
	1:       
	
/  0 "" 2
/NO_APP
	movw	%dx, -4(%ebp)
	movw	%ax, -2(%ebp)
	cmpw	$0, -4(%ebp)
	jne	.L12
	movzwl	-2(%ebp), %eax
	jmp	.L14
.L12:
	movl	$0, %eax
.L14:
	leave
	ret
	.size	bios_get_low_memory_in_kb, .-bios_get_low_memory_in_kb
	.globl	bios_detect_memory_map
	.type	bios_detect_memory_map, @function
bios_detect_memory_map:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	subl	$20, %esp
	movw	$0, -18(%ebp)
	movl	$0, -16(%ebp)
.L17:
/APP
/  173 "main.c" 1
	mov $0xE820, %eax   
	mov 8(%ebp), %di   
	mov -16(%ebp), %ebx   
	mov $24, %ecx 
	mov $0x534D4150, %edx 
	int $0x15      
	cmp $0x534D4150, %eax 
	jne 1f 
	jc 1f 
	jmp 2f 
	1: 
	movw $1, -30(%ebp) 
	2:  
	mov %ebx, %esi 
	
/  0 "" 2
/NO_APP
	movzwl	-30(%ebp), %eax
	movw	%ax, -18(%ebp)
	movl	%esi, -16(%ebp)
	cmpw	$0, -18(%ebp)
	jne	.L16
	cmpl	$0, -16(%ebp)
	je	.L16
	addl	$24, 8(%ebp)
	jmp	.L17
.L16:
	cmpw	$0, -18(%ebp)
	je	.L18
	movl	$-1, %eax
	jmp	.L20
.L18:
	movl	$0, %eax
.L20:
	addl	$20, %esp
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	bios_detect_memory_map, .-bios_detect_memory_map
	.ident	"GCC: (GNU) 11.2.0"
