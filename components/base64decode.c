#include <stdio.h>

int char2id(char c) {
	static const char *table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int i;
	for(i=0;table[i]!='\0';i++) {
		if(table[i]==c)return i;
	}
	return -1;
}

int get_next_char(void) {
	for(;;) {
		int input=getchar();
		int id;
		if(input==EOF)return -1;
		id=char2id(input);
		if(id>=0)return id;
	}
}

int base64_get_next_byte(void) {
	static int buffer;
	static int buffer_pos=0;
	int ret=-1;
	int next=get_next_char();
	if(next<0)return -1;
	if(buffer_pos==0) {
		ret=(next<<2)&0xfc;
		if((next=get_next_char())<0)return -1;
		ret|=(next>>4)&0x03;
		buffer=(next<<4)&0xf0;
		buffer_pos=1;
	} else if(buffer_pos==1) {
		ret=buffer|((next>>2)&0x0f);
		buffer=(next<<6)&0xc0;
		buffer_pos=2;
	} else if(buffer_pos==2) {
		ret=buffer|(next&0x3f);
		buffer_pos=0;
	}
	return ret;
}

int main(int argc,char *argv[]) {
	FILE* out;
	int input;
	if(argc!=2) {
		fputs("please specify output file name\n",stderr);
		return 1;
	}
	out=fopen(argv[1],"wb");
	if(out==NULL) {
		fputs("output file open error\n",stderr);
		return 1;
	}
	while((input=base64_get_next_byte())>=0)putc(input,out);
	fclose(out);
	return 0;
}
