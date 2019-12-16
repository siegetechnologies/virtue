#include <stdio.h>
#include <stdlib.h>
int main(const int, const char **);

int __attribute__ ((noinline)) smashfun(void){
	char bof[64];
	fgets(bof, 128, stdin);
	return 0;
}

int main(const int argc, const char ** argv){

	printf("Lets segfault this thing with a stack overflow\n");
	smashfun();
	printf("If you see this, then we didnt segfault!\n");
	return 0;
}


