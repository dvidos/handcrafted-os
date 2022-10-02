# historical notes

Some notes on historical facts, as we discover them.

## on argv

It seems to me that the original idea was to pass 
the arguments of the shell as pointers to strings
in the main() function of the executed program.

Maybe due to so many arguments, or due to wanting 
to pass the environment as well, they converted
to passing a pointer to a table of pointers.

The original implementation was pushing all the pointers
on stack (which hints at passing many string pointers),
but then also passing the address of the table of the stack
to the function, i.e. the argv and envp pointers pointed 
to stack entries.

## on device numbers

Reading the Lions book, it seems the original 
code had an array of pointers to structures, where
each structure had pointers to functions for device manipulation.

Actually, there was one array for block devices 
and a different one for character devices.

Device major number would correspond to the index of the array,
hence, we could have a hdd on offset 1 and a tty on offset 2.
By using major as index, we had a form of polymorphism,
based on the device type.

```
    openi(*ip, rw)
    {
        dev = ip->i_addr[0];
        maj = ip->i_addr[0].d_major;

        switch(ip->i_mode & IFMT) {
            case IFCHR:
                (*cdevsw[maj].d_open)(dev, rw);
                break;
            case IFBLK:
                (*bdevsw[maj].d_open)(dev, rw);
                break;
        }
        ...
```

