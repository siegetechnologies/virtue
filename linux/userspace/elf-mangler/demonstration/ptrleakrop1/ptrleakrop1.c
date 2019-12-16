#include <stdio.h>
#include <stdlib.h>

void __attribute__ ((noinline)) shellfun(void){
	system("/bin/sh");
}



int __attribute__ ((noinline)) leakfun(void){
	printf("Leaking a ptr to leakfun: %p\n", leakfun);
	return 0;
}

int __attribute__ ((noinline)) smashfun(void){
	char bof[64];
	fgets(bof, 128, stdin);
	return 0;
}

int main(void){
	printf("Goal is to run shellfun.\nWe can leak leakfun's addr, and have a stack smash in smashfun\n");
	leakfun();
	printf("delta between leaked ptr and smashfun.... %li\n", (void*)shellfun - (void*)leakfun);
	smashfun();
	return 0;
}


