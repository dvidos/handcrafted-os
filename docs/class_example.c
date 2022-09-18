/**
 * This is an example of how a class oriented solution can be implemented in C.
 * The aim is simplicity, not ultimate OOP flexibility.
 * 
 * There is a structure that represents the instance of an object.
 * 
 * Each instance has a collection of pointers to functions, 
 * which represent the methods of this class. 
 * The actual function implementation is declared with the `static` keyword,
 * therefore hiding it from external modules.
 * 
 * The signature of the function pointers provides the "contract"
 * against which the caller will code. Our ability to setup 
 * different functions on those pointers depending on the case, 
 * provides prolymorphism. This is how VFS is implemented.
 * 
 * Each instance also has a pointer to a `private_info` structure.
 * This structure, while `typedef`'d, is not defined in the shared header.
 * Its contents are therefore hidden to external modules.
 * This is used for any private information and methods the class has.
 * 
 * Finally, there is a pair of methods, the constructor and destructor.
 * The constructor that will allocate memory and initialize this structure.
 * It can take arguments (e.g. pointers to other objects).
 * The destructor is responsible for freeing the memory allocated by the constructor.
 * 
 * We can pass various values to the constructor, including pointers
 * to other, lower level "classes". This way, we can implement 
 * dependency inversion, a powerful technique which allows for 
 * unit testing, as well as implementing e.g. the loop devices.
 * 
 * What we call "class" methods and atributes in OOP, can be implemented
 * using static functions and data in the C file.
 * 
 * In our example here, we name our class "abcd". 
 * All structs and types have the classname included
 * 
 * Maybe in the future, we can derive some macros,
 * for more uniform and eaier declarations.
 */


/* -------------------------------
 * Contents of the "abcd.h" file
 * ------------------------------- */

typedef struct abcd_ops_struct abcd_ops_t;
typedef struct abcd_private_info_struct abcd_private_info_t;
typedef struct abcd_struct abcd_t;

/* operations structure, only function pointers */
struct abcd_ops_struct {
    /* pointers to functions, taking a abcd_t pointer as first argument */
    int (*open)(abcd_t *abcd, char *name);
    int (*read)(abcd_t *abcd, int hndl, char *buffer, int len);
    int (*write)(abcd_t *abcd, int hndl, char *buffer, int len);
    int (*close)(abcd_t *abcd, int hndl);
    /* other public methods here */
};

/* the actual struct, akin to a class */
struct abcd_struct {
    abcd_ops_t *ops;
    abcd_private_info_t *priv;
    /* other public attributes here */
};

abcd_t *create_abcd(int initial_value);
abcd_t *destroy_abcd(abcd_t *abcd);

/* -------------------------------
 * Contents of the "abcd.c" file
 * ------------------------------- */

/* notice private data declaration inside the C file only */
struct abcd_private_info_struct {
    int value;
    /* other private methods and data here */
};

static int open(abcd_t *abcd, char *file) {
    // ...
}

static int read(abcd_t *abcd, int hndl, char *buffer, int length) {
    // ...
}

static int write(abcd_t *abcd, int hndl, char *buffer, int length) {
    // ...
}

static int close(abcd_t *abcd, int hndl) {
    // ...
}

abcd_t *create_abcd(int initial_value) {

    abcd_ops_t *ops = malloc(sizeof(abcd_ops_t));
    ops->open = open;
    ops->read = read;
    ops->write = write;
    ops->close = close;

    abcd_private_info_t *priv = malloc(sizeof(abcd_private_info_t));
    priv->value = initial_value;

    abcd_t *abcd = malloc(sizeof(abcd_t));
    abcd->ops = ops;
    abcd->priv = priv;
    return abcd;
}

abcd_t *destroy_abcd(abcd_t *abcd) {
    if (abcd->priv) {
        // free private members if needed
        free(abcd->priv);
    }
    if (abcd->ops) {
        free(abcd->ops);
    }
    free(abcd);

    // return NULL to allow caller nullify the variable in one call:
    // ptr = destroy_abcd(ptr);
    return NULL;
}

