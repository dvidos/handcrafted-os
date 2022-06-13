
# Programming patterns

## programming to a contract or interface

In an OOP language, we use interfaces or base classes in various patterns 
(e.g. strategy, command etc), to program to the interface
and decouple the caller from it.

In C, we can do the same thing using a function pointer.
The type (or typedef) of the pointer is the interface,
and the caller does not know the actual function
it is calling.

This allows pointers even to `static` methods, implementing 
some sort of name hiding, the same way that in an OOP
two methods in different classes can have the same name,
we can have the same here, different `static` methods
in different C source files.


## information hiding

In OOP languages, we denote some fields as private or protected,
so the compiler does not allow external code to "see" them.

In C, we can implement the same using `static` variables in
a source file. Only exported methods can access those variables,
therefore, we implement the same logic.

If more than just global variables is needed, 
we can declare a `struct` without difining it (e.g. `struct x;`).
This allows us to export functions with pointers to this structure,
without external code ever knowing the contents of that structure.


