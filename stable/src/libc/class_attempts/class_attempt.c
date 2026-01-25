#include <ctypes.h>
#include <va_list.h>

// my original approach was different:
// - have a constructor function allocate and return a pointer to the object
// - the object will have an "ops" member with pointers to functions.
// - this might not support inheritence or polymorphism, but it's clean enough.

// based on this pdf: https://www.cs.rit.edu/~ats/books/ooc.pdf
// immitation of c++ classes implementation, before getting to the ugly side of syntax.

// we are supposed to create one instance of this structure
// for each class we are interested in creating.
struct type_description {
    size_t size;
    char *name;
    void *(*costructor)(void *this, va_list *args);
    int (*destructor)(void *this);
    void *(*clone)(void *this);
    int (*compare)(void *this, void *that);
};

void *new(struct type_description *td, ...) {
    // allocate
    void *p = kmalloc(td->size);
    assert(p);

    // assign type_description (type descriptor must be the first member of the object)
    *(struct type_description **)p = td;

    // initialize, if applicable
    if (td->costructor) {
        va_list args;
        va_start(args, td);
        void *obj = td->costructor(p, args);
        va_end(args);
    }

    return p;
}

// polymorphic methods such as this one, expose late binding
void *clone(void *object) {
    if (object == NULL)
        return NULL;
    
    struct type_description *td = *(struct type_description **)object = td;
    if (td && td->clone) {
        return td->clone(object);
    }

    // else what??
    return NULL;
}

int compare(void *a, void *b) {
    if (a == NULL && b == NULL) return 0;
    if (a == NULL && b != NULL) return -1;
    if (a != NULL && b == NULL) return 1;

    struct type_description *td_a = *(struct type_description **)a;
    if (td_a->compare) {
        return td_a->compare(a, b);
    }

    // else what?
    return INT32_MAX;
}

void delete(void *object) {
    if (object == NULL)
        return;

    struct type_description *td = *(struct type_description **)object;
    if (td && td->destructor) {
        td->destructor(object);
    }

    kfree(object);
}


// ------- specific class code ----------------------------------

typedef struct rectangle {
    struct type_description *type; // important to be first
    int width;
    int height;
};
void rect_constructor(void *this, va_list args) {
    struct rectangle *rect = (struct rectangle *)this;

    rect->height = va_arg(args, int);
    rect->width = va_arg(args, int);
}
void * rect_clone(void *this) {
    struct rectangle *rect = (struct rectangle *)this;
    return new(Rectangle, rect->height, rect->width);
}
void rect_compare(void *a, void *b) {
    struct rectangle *rect_a = (struct rectangle *)a;
    struct rectangle *rect_b = (struct rectangle *)b;

    int area_a = rect_a->height * rect_b->width;
    int area_b = rect_b->height * rect_b->width;

    return area_a - area_b;
}
void rect_destructor(void *this) {
    struct rectangle *rect = (struct rectangle *)this;
    // nothing actually!
}

static struct type_description rectangle_type = {
    .size = sizeof(struct rectangle),
    .name = "rectangle",
    .costructor = rect_constructor,
    .destructor = rect_destructor,
    .clone = rect_clone,
    .compare = rect_compare
};
struct type_description *Rectangle = &rectangle_type;


// ------- demo code ----------------------------------

void demo() {
    struct rectangle *r1 = new(Rectangle, 3, 4);
    struct rectangle *r2 = new(Rectangle, 2, 5);
    struct rectangle *r3 = clone(r2);
    int answer = compare(r2, r3);
    delete(r1);
    delete(r2);
    delete(r3);
}

// from here on, it's specific syntax, so it turns ugly. 
// let's try the idea of a pointer to object,
// having a pointer to class, having a pointer to super class etc,
// but pointers to functions are resolved at constructor time.
// therefore the object shall have a pointer to the class pointers table.

