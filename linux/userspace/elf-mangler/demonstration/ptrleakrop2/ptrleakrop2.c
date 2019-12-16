#include <stdio.h>
#include <stdlib.h>

int __attribute__ ((noinline)) leakfun(void){
	char doof[64];
	printf("Leaking a ptr to somewhere in main: %p\n",
		((void**)doof)[9]
		);
	return 0;
}

int __attribute__ ((noinline)) smashfun(void){
	char bof[64];
	fgets(bof, 128, stdin);
	return 0;
}

int main(const int argc, const char ** argv){

	if( argc < 1){
		//goal is to jump here
		system("/bin/sh");
	}

	printf("Goal is to jump past the check that will normally not execute and run shellfun.\nWe can leak a return address into main with leakfun\n");
	leakfun();
	printf("Now we can smash\n");
	smashfun();
	return 0;
}


