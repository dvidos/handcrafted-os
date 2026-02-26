# libc

This is the library against which the user land programs shall be linked against.

The aims are:

* make it POSIX like, to make porting of various programs to our OS easier.
* add experimental features (e.g. containers, predicates, etc)
* add special features of our OS only (e.g. working with json objects and streams)

We shall need tests as well...