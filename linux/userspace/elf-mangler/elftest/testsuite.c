#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char * str1 = "I am a string up here\n";

int addnum(int a, int b){
	return a+b;
}

int switcher(int c){
	switch(c){
		case 0:
			printf("Yolo\n");
		case 1:
			printf("bah %i\n", c);
			break;
		case 2:
			printf("doing a calc\n");
			int j = c + 123;
			printf("j i %i\n", j);
			break;
		case 3:
			printf("another thing\n");
		case 4:
			printf("this is a 4rth thing\n");
		case 6:
			printf("I hope this jump table doesnt get broken\n");
		break;
		default:
			printf("YOU GAVE ME A BAD ONE\n");
		break;
	}
	return c;
}
char * stackarg_test(char * in1, char * in2, char * in3, char * in4, char * in5, char * in6, char * in7, char * in8, char * in9, char * in10, int which){
	char whoosit[100] = {0};
	switch(which){
		case 1:
			strcpy(whoosit, in1);
			return in1;
		case 2:
			return in2;
		case 3:
			strcpy(whoosit, in3);
			return in3;
		case 4:
			return in4;
		case 5:
			return in5;
		case 6:
			return in6;
		case 7:
			return in7;
		case 8:
			return in8;
		case 9:
			return in9;
		case 10:
			return in10;
		default: return NULL;
	}
	return NULL;

}



char *fakeargs[] = {"Bender", "Flexo", "Calculon", "Donbot", "Clamps", "Hedonismbot", "Stephen Hawking", "Reverend Lionel Preacherbot", "Robot Devil", "Robot Santa", "Tinny Tim", "URL"};

int main(void){
	srand(0);
	printf("I am test #1, i dont want to crash!\n");
	printf("%s", str1);
	printf("result of rand %i\n", rand());
	int a = rand()/100;
	int b = rand()/100;
	printf("result of addnum(%i, %i) = %i\n", a, b, addnum(a,b));
	int c = rand() %4;
	printf("gonna switcher with %i\n", c);
	switcher(c);
	c = rand() %4;
	printf("gonna switcher with %i\n", c);
	switcher(c);
	c = rand() %4;
	printf("gonna switcher with %i\n", c);
	switcher(c);
	c = rand() %7;
	printf("gonna switcher with %i\n", c);
	switcher(c);

	int i;
	for(i = 0; i < 10; i++){
		int rn = rand()%11;
		printf("gonna lottatest with %i\n", rn);
		printf("%s\n", stackarg_test(fakeargs[0], fakeargs[1], fakeargs[2], fakeargs[3], fakeargs[4], fakeargs[5], fakeargs[6], fakeargs[7], fakeargs[8], fakeargs[9], rn));
	}
	return 0;
}
