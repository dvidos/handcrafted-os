
# libc

This folder generates two libraries, one for user programs, one for the kernel.

## libc.a

This is the library for the user programs. It is compiled having the `__is_libc` symbol defined.

## kibk.a

This is the library for the kernel. It is compiled with having the `__is_libk` symbol defined.

## output

The output of this project is the header files and the libraries.

* `sysroot/usr/include` - all header files to compile programs against
* `sysroot/usr/lib` - libc.a and libk.a files to link programs against


## to implement

```c
// macros:

min(), max()
UINT_MAX etc limits



// code:

exit()
exec()


malloc()
free()

open()
seek()
read()
write()
close()
flush()

opendir()
readdir()
closedir()

stat()
mkdir()
touch()
unlink()

getpid()
exit()
getenv()
setenv()
// fork(), wait() under discusion
// raise(), signal() under discussion

gets()
puts()
getchar()
putchar()
printf()

sprintf()
vsprinf() etc

atoi()
atol()
atou()
ltoa()
itoa()
utoa()

strxxx() family
memxxx() family
wcsxxx() wide char family
isalpha(), isdigit() etc
tolower(), toupper() etc

random()  later on
bsearch() later on
qsprt()   later on

get_clock()
uptime() 

```
* open()
* write()
* 