#include <stdio.h>
#include <stdlib.h>
int main(const int, const char **);

int __attribute__ ((noinline)) leakfun(void){
	printf("Leaking a ptr to somewhere in main: %p\n", main
		);
	return 0;
}

int __attribute__ ((noinline)) smashfun(void){
	char bof[64];
	fgets(bof, 1024, stdin);
	return 0;
}

int main(const int argc, const char ** argv){

	printf("Goal is to ROP to gain system(/bin/sh)\n");
	leakfun();
	printf("Now we can smash\n");
	smashfun();
	return 0;
}


