#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "stringlib.h"

#define TRUE 1
#define FALSE 0

int curplace = 0;
int *data = 0;
int datasize = 0;
int maxput = 0;


typedef struct label_s {
	char * name;
	int type;
	int pos;
	struct label_s * uses;
} label_t;


label_t * labeltable = 0;
int labeltablesize = 0;

int writelabels(label_t *ls, size_t size){
	int i;
	for(i = 0; i < size; i++){
		label_t *l = labeltable+i;
		if(!l->type) continue;
		printf("%s: %i\n", l->name, l->pos);
	}
	return i;
}

int deletelabel(label_t * l){
	int cnt;
	label_t *n;
	for(n = l->uses, cnt = 0; n; cnt++){
		l = n;
		n = l->uses;
		free(l);
	}
	return cnt;
}
int addlabel(char * name, int pos){
	int len = strlen(name);
	if(ISALPHANUM(name[len-1]))len++; //uhhh should have a : at the end, but idk
	char * newname = malloc(len);
	memcpy(newname, name, len);
	newname[len-1] = 0;
	//search for preexisting
	//todo use a hashmap but i am laze
	int i;
	for(i = 0; i< labeltablesize; i++) if(string_testEqual(newname, labeltable[i].name)) break;
	label_t *l;
	//got to the end without finding one
	if(i == labeltablesize){ //doesnt exist yet, add it
		labeltablesize++;
		labeltable = realloc(labeltable, labeltablesize * sizeof(label_t));
		label_t *l = &labeltable[i];
		l->name = newname;
		l->type = 1;
		l->pos = pos;
		l->uses = 0;
		return TRUE;
	} else {	//already exists in
		if(newname)free(newname);
		l = &labeltable[i];
		if(!l->type){ //has not been defined yet
			label_t *n = malloc(sizeof(label_t));
			memcpy(n, l, sizeof(label_t));
			l->uses = n;
			l->type = 1;
			l->pos = pos;
			return TRUE;
		}
	}
	return FALSE;
}


int addlabelref(char * name, int pos){
	int len = strlen(name);
	if(ISALPHANUM(name[len-1]))len++; //uhhh shouldnt have a : at the end, but idk
	char * newname = malloc(len);
	memcpy(newname, name, len);
	newname[len-1] = 0;

	//search for preexisting
	//todo use a hashmap but i am laze
	int i;
	for(i = 0; i< labeltablesize; i++) if(string_testEqual(newname, labeltable[i].name)) break;
	label_t *l;
	//got to the end without finding one
	if(i == labeltablesize){ //doesnt exist yet, add it
		labeltablesize++;
		labeltable = realloc(labeltable, labeltablesize * sizeof(label_t));
		label_t *l = &labeltable[i];
		l->name = newname;
		l->type = 0;
		l->pos = pos;
		l->uses = 0;
		return FALSE;
	} else {	//already exists in
		if(newname)free(newname);
		l = &labeltable[i];
		label_t *n = malloc(sizeof(label_t));
		if(!l->type){ //has not been defined yet
			memcpy(n, l, sizeof(label_t));
			l->uses = n;
			l->type = 0;
			l->pos = pos;
			return FALSE;
		} else { //has already been defined
			n->name = l->name;
			n->type = 0;
			n->pos = pos;
			n->uses = l->uses;
			l->uses = n;
			return TRUE;
		}
	}
	return FALSE;
}



void putdata(int in, int place){
	if(place >= datasize){
		datasize = place +16;
		data = realloc(data, datasize * sizeof(int));
	}
	if(place > maxput) maxput = place;
	data[place] = in;
}

int lastop = 0;

const char *hardcodes[] = {"PREVA", "PREVB", "PREVC", "THISA", "THISB", "THISC", "NEXTA", "NEXTB", "NEXTC", 0};
const int hardoffsets[] = {	-3,	 -2,	  -1,	    1,	     2,	      3,       5,	6,	 7, 0};
int writehardcode(char * word){
	//look at the last op (in a case of add CONST1 Array NEXTA, it will be the add), and add the offset onto it
	//does not work with hug-ery, but whatever
	int ofs;
	for(ofs = 0; hardcodes[ofs]; ofs++) if(string_testEqualWCI(hardcodes[ofs], word)) break;
	if(!hardcodes[ofs]) return FALSE;
	int bmp = strlen(hardcodes[ofs]);
	//add offset onto last instruction
	int ptr = lastop + hardoffsets[ofs];
	putdata(ptr, curplace);
	curplace++;
	return bmp;
}

const char *ops[] = {"nop", "add", "sub", "mult", "div", "mod", "and", "or", "xor", "not", "bnot", "sl", "sr", "sa", "tg", "te", 0};
//if it isnt a valid op, returns false
//returns number of characters that the string was
int writeop(char * word){
	int opcode;
	for(opcode = 0; ops[opcode]; opcode++) if(string_testEqualWCI(ops[opcode], word)) break;
	if(!ops[opcode]) return FALSE;
	int bmp = strlen(ops[opcode]);
//	opcode = opcode << 3;
//	int i;
//	for(i = 0; i < 3 && !ISWHITESPACE(word[bmp + i]); i++){
//		if(ISUPPER(word[bmp+i])) opcode = opcode | 1<<i;		//will change later
//	}
	putdata(opcode, curplace);
	lastop = curplace;
	curplace++;
	return bmp;
//	return bmp+i;
}
char * tword = 0;
unsigned int twordlen = 0;
int parseword(char * word){
	if(string_testEqualN(word, "//", 2)){ // check if comment
		int cnt;
		for(cnt = 0;word[cnt] && word[cnt] != '\n'; cnt++);
		return cnt+0;
	}
	if(string_testEqualN(word, "/*", 2)){ // */ check if longcomment
		int cnt;
		for(cnt = 0; word[cnt] && !string_testEqualN(word + cnt, "*/", 2); cnt++);
		return cnt+1;
	}
	int sz = string_wordLength(word);
	if(sz >= twordlen){
		twordlen = sz+1;
		if(tword) free(tword);
		tword = malloc(twordlen);
	}
	int i;
	for(i = 0; i < sz; i++)tword[i] = word[i];
	tword[sz] = 0;

	int writeres = writeop(word);
	if(writeres){
		printf("parsed %s as the op %i or 0x%08x, loc %i\n", tword, data[curplace-1], data[curplace-1], curplace-1);
		return writeres;
	}
	writeres = writehardcode(word);
	if(writeres){
		printf("parsed %s as the HARDCODE to point to loc %i, loc %i\n", tword, data[curplace-1], curplace-1);
		return writeres;
	}

	if(string_testEqualN(word,"0x", 2)){ //hex value
		errno = 0;
		writeres = strtol(word, NULL, 0);
	}
	else if(ISALPHA(tword[0])){
		if(tword[sz-1] == ':'){
			if(addlabel(tword, curplace)) printf("parsed %s as label to %i or 0x%08x\n", tword, curplace, curplace);
			else printf("error, label %s already exists\n", tword);
			return sz;
		}
		if(addlabelref(tword, curplace)) printf("parsed %s as label ref\n", tword);
		else printf("parsed %s as label ref, label does not already exist\n", tword);
		putdata(-1, curplace);
		curplace++;
		return sz;
	} else errno = 1;

/*	if(errno){
		errno = 0;
		writeres = strtof(word, NULL) + 1.0;
	}*/
	if(errno){ //int?
		errno = 0;
		writeres = strtol(word, NULL, 10);
	}

	if(errno){
		printf("error parsing %s\n", tword);
		writeres = 0;
	}
	else printf("parsed %s as %i or 0x%08x, loc %i\n", tword, writeres, writeres, curplace);
	putdata(writeres, curplace);
	curplace++;

	return sz;
}




int postlabels(void){
	int i;
	for(i = 0; i < labeltablesize; i++){
		label_t *l = &labeltable[i];
		label_t *n;
		if(!l->type){
			printf("ERROR: label %s undefined. Uses:\n", l->name);
			for(n = l; n; n=n->uses){
				printf("\t %i or 0x%08x\n", n->pos, n->pos);
			}
		} else {
			printf("label %s defined at %i or 0x%08x. Uses:\n", l->name, l->pos, l->pos);
			for(n = l->uses; n; n=n->uses){
				printf("\t %i or 0x%08x\n", n->pos, n->pos);
				putdata(l->pos, n->pos);
			}
		}
		if(l->uses)deletelabel(l->uses);
		if(l->name)free(l->name);
	}
	return i;
}

char * indata = 0;
size_t insize = 0;
int main(int argc, char ** argv){
	FILE *f = fopen(argv[1], "r");
	fseek(f, 0, SEEK_END);
	insize = ftell(f);
	rewind(f);
	indata = malloc(insize+1);
	fread(indata, 1, insize, f);
	indata[insize] = 0;
	fclose(f);
	char * curln = indata;
	while(*curln){
		curln += parseword(curln);
		//advance to next word
		for(;!ISWHITESPACE(*curln); curln++);
		for(;*curln && ISWHITESPACE(*curln); curln++);
	}
	writelabels(labeltable, labeltablesize);
	postlabels();

	f = fopen(argv[2], "wb");
	fwrite(data, (maxput+1) * sizeof(int), 1, f);




	if(indata) free(indata);
	if(data)free(data);
	if(tword)free(tword);
	if(labeltable) free(labeltable);
	return FALSE;
}
