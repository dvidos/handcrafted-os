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

