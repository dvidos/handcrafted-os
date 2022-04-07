#include <stdio.h>
#include <stdlib.h>

/**
 * Sample class definition -- too c++ for my taste
 * Maybe the original file io struct is better design for C
 *
 * Usage:
 *
 *   Class *c = newClass(10);
 *   int j = c->method1(c, 1, 2);
 *   printf("j is now %i\n", j);
 */
typedef struct Class_struct Class;
typedef int Class_pf_iii(Class *this, int arg1, int arg2);

struct Class_struct {
	int base;
	Class_pf_iii *method1;
};

int Class_method1(Class *this, int arg1, int arg2) {
	return this->base + arg1 + arg2;
}

Class *newClass(int base) {
	Class *p = malloc(sizeof(Class));
	p->base = base;
	p->method1 = Class_method1;
	return p;
}





int main(char *argv, int argc) {
	printf("Hello, mits!\n");

	char *p = malloc(1000);
	printf("Gotten a memory chunk!\n");
	free(p);


	Class *c = newClass(10);
	int j = c->method1(c, 1, 2);
	printf("j is now %i\n", j);

	int i;
	printf("sizeof(int) is %ld\n", sizeof(int));
	printf("sizeof(short) is %lu\n", sizeof(short));
	printf("sizeof(long) is %lu\n", sizeof(long));
	printf("sizeof(long long) is %lu\n", sizeof(long long));


	for (i = 0; i < 10; i++) {
		printf("%d... ", i);
	}
	printf("done\n");


}

