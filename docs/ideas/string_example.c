/**
 * Since dynamic memory allocation and deallocation is a problem in pure C,
 * let's try for a string object that can work with internal buffer 
 * and possible expand / shrink itself as needed.
 * 
 * Something similar to the StringBuilder of C# or similar classes in other langs.
 */



/* -------------------------------
 * Contents of the "string.h" file
 * ------------------------------- */

typedef struct string_ops_struct string_ops_t;
typedef struct string_private_info_struct string_private_info_t;
typedef struct string_struct string_t;

/* operations structure, only function pointers */
struct string_ops_struct {
    // return pointer to zero terminated string
    char *(*strptr)(string_t *string);

    // return length of string (e.g. strlen())
    int (*length)(string_t *string);

    // modifications in place
    void (*clear)(string_t *string);
    void (*append)(string_t *string, char *str);
    void (*insert)(string_t *string, int index, char *str);
    void (*delete)(string_t *string, int index, int length);

    // create and return a new substring - caller must destroy it
    string_t *(*substring)(string_t *string, int index, int length);
};

/* the actual struct, akin to a class */
struct string_struct {
    string_ops_t *ops;
    string_private_info_t *priv;
    /* other public attributes here */
};

string_t *create_string(int initial_capacity);
void destroy_string(string_t *string);

/* -------------------------------
 * Contents of the "string.c" file
 * ------------------------------- */

/* notice private data declaration inside the C file only */
struct string_private_info_struct {
    char *buffer;    // allocated buffer
    int buffer_size; // allocated size
    int length;      // string length
};

static char *strptr(string_t *string) {
    return string->priv->buffer;
}

int length(string_t *string) {
    return string->priv->length;
}

static void clear(string_t *string) {
    string->priv->buffer[0] = 0;
    string->priv->length = 0;
}

static void append(string_t *string, char *str) {
    // ensure space, reallocate as needed
}

static void insert(string_t *string, int index, char *str) {
    // ensure space, reallocate as needed
}

static void delete(string_t *string, int index, int length) {
    // fail silently if numbers don't make sense
}

static string_t *substring(string_t *string, int index, int length) {
    // need to create a new substring
    return NULL;
}

string_t *create_string(int initial_capacity) {

    string_ops_t *ops = malloc(sizeof(string_ops_t));
    ops->strptr = strptr;
    ops->length = length;
    ops->clear = clear;
    ops->append = append;
    ops->insert = insert;
    ops->delete = delete;
    ops->substring = substring;

    string_private_info_t *priv = malloc(sizeof(string_private_info_t));
    priv->buffer_size = initial_capacity + 1;
    priv->buffer = malloc(priv->buffer_size);
    priv->length = 0;

    string_t *string = malloc(sizeof(string_t));
    string->ops = ops;
    string->priv = priv;
    return string;
}

void destroy_string(string_t *string) {
    if (string->priv) {
        if (string->priv->buffer)
            free(string->priv->buffer);
        free(string->priv);
    }
    if (string->ops) {
        free(string->ops);
    }
    free(string);
}

