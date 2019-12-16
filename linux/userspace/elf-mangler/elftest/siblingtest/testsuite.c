#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char * str1 = "I am a string up here\n";

char * __attribute__ ((noinline)) sis(void){
	return str1;
}

char * bro(int test){
	int c1;
	int c2;
	int c3;
	c1 =rand();
	c2 = rand();
	c3 = rand();
	if(!test || c1 == c2 || c2 == c3 || c1 == c3) return 0;
	return sis();
}



int main(void){
	printf("%s\n", bro(1));
	return 0;
}
