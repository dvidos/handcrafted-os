#include <stdio.h>
#include <stdlib.h>



int main(char *argv, int argc) {
	printf("Hello, mits!\n");

	char *p = malloc(1000);
	printf("Gotten a memory chunk!\n");
	free(p);

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

