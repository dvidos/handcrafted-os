# Data Structures

I'm interested in creating data structures.

## Information Hiding

I did not want to create a whole series of functions to work 
with a structure, I wanted code completion to work for the user.

What I came up with consists of the following four parts:

* One **public structure** with pointers to the functions that perform operations on the data structure. The structure is also passed as the first argument to those functions.
* One **factory method** `create_...()` to create the data structure. It returns a pointer to the public structure.
* One **private data hook** `void *priv_data` member in the public structure, to allow the functions to hold any private, encapsulated data
* The **implemented methods**, defined as `static` in the source code, only visible to the factory method.

The above creates a very clean interface for callers, that allows the following for example:

```c
    struct item_operations *str_ops = create_str_item_operations();
    list_t *list = create_list(str_ops);
    list->add(list, "Item 1");
    list->add(list, "Another item");
    list->add(list, "The final item");
    list->for_each(list, print_list_item);
```

## Item Operations

The polymorphism enabling idea is the `item_operations`, a structure that allows one
to use a polymorphic `void *` pointer, yet support various operations:

* equals - to allow values and identity based equality checks
* clone - to allow deep cloning of complex objects
* free  - to allow deep freeing complex objects
* matches - to allow custom matching capabilities
* compare - to allow comparisons for sorting etc
* hash - to allow to be used as a key in a hashtable

The above allows us to create data structures for strings, binary structures, 
even possibly by implementing the `Variant` data type, provided we support the above operations.
And all that in a strongly typed language such as C!

To create the array of operations, corresponding public functions exist:

* `create_str_item_operations()`
* `create_mem_item_operations()`

As the above methods allocate memory, the corresponding `free...()` methods 
must also be called when we are done with the operations.

In a way, the operations is like describing a specific class, 
it allows us to work with any data pointed by `void *`, without 
knowing its type.

## Data Structures Implementation

With the above operations in place, we can create various data structures.
For the time being I'm aiming at:

* list - generic list with `O(n)` operations
* hashtable - string keyed hashtable with `O(1)` operations
* btree - a binary tree with `O(lg n)` operations

The mechanism to implement the above is one public struct,
with pointers to methods for implementing the functionality.
The public struct also has a `void *priv_data` pointer for private data,
which each data struture uses to hold data to be hidden by callers.

This way code completion works wonderfully, by using the public pointer.

To create the data structures, there are factory functions,
where one passes in the `operations` structure, along with any remaining arguments.

* `create_list()`
* `create_hashtable()`
* `create_btree()`

Corresponding free methods exist of course.

# On objects with C

Very nice material on classes and objects using C is here:
https://www.cs.rit.edu/~ats/books/ooc.pdf

Very briefly, each class is a structure that has a size, ctor pointer (a var args method),
a dtor pointer, a comparer and a cloner.

There is one generic function `new(class, ...)` in which we pass the class.
The memory for the new instance is allocated, the class pointer is 
assigned (convention is that the first member of an object is a pointer to the class) 
and the constructor is called, if present. Then the object is returned.

There is one generic function `delete(obj)` in which we pass the object,
and it will call the appropriate destructor, then free the memory.
Same, there are generic functions `clone()` and `differ()` which
use the class to see if there are appropriate methods to clone, compare etc.

Most of the object methods are pointers on the class structure, not the object
structure. (e.g. they don't change from instance to instance). The data are indeed
part of the object structure (as they change from instance to instance)

At some point, he breaks out an awk preprocessor, because the syntax
becomes so complex that it's not nice to maintain manually...
This goes beyond what I'd like to achieve. 