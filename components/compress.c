#include <stdio.h>

void compress(FILE* in, FILE* out) {
	static unsigned char data_buf[65526];
	unsigned char number_buf[4];
	int count=0;
	int zero_mode=-1;
	int input;
	while((input=getc(in))!=EOF) {
		if(input==0) {
			if(zero_mode==1) {
				count++;
			} else {
				if(count>0) {
					if(zero_mode==1) {
						number_buf[0]=((count>>24)&0x7f)|0x80;
						number_buf[1]=(count>>16)&0xff;
						number_buf[2]=(count>>8)&0xff;
						number_buf[3]=count&0xff;
						fwrite(number_buf,4,1,out);
					} else if(zero_mode==0) {
						number_buf[0]=(count>>8)&0x7f;
						number_buf[1]=count&0xff;
						fwrite(number_buf,2,1,out);
						fwrite(data_buf,count,1,out);
					}
				}
				zero_mode=1;
				count=1;
			}
		} else {
			if(zero_mode==0 && count<0x7fff) {
				data_buf[count++]=input;
			} else {
				if(count>0) {
					if(zero_mode==1) {
						number_buf[0]=((count>>24)&0x7f)|0x80;
						number_buf[1]=(count>>16)&0xff;
						number_buf[2]=(count>>8)&0xff;
						number_buf[3]=count&0xff;
						fwrite(number_buf,4,1,out);
					} else if(zero_mode==0) {
						number_buf[0]=(count>>8)&0x7f;
						number_buf[1]=count&0xff;
						fwrite(number_buf,2,1,out);
						fwrite(data_buf,count,1,out);
					}
				}
				zero_mode=0;
				data_buf[0]=input;
				count=1;
			}
		}
	}
	if(count>0) {
		if(zero_mode==1) {
			number_buf[0]=((count>>24)&0x7f)|0x80;
			number_buf[1]=(count>>16)&0xff;
			number_buf[2]=(count>>8)&0xff;
			number_buf[3]=count&0xff;
			fwrite(number_buf,4,1,out);
		} else if(zero_mode==0) {
			number_buf[0]=(count>>8)&0x7f;
			number_buf[1]=count&0xff;
			fwrite(number_buf,2,1,out);
			fwrite(data_buf,count,1,out);
		}
	}
}

void decompress(FILE* in, FILE* out) {
	int input;
	int num;
	int i;
	while((input=getc(in))!=EOF) {
		if(input & 0x80) {
			num = input & 0x7f;
			num = (num << 8) | getc(in);
			num = (num << 8) | getc(in);
			num = (num << 8) | getc(in);
			for(i=0;i<num;i++)putc(0,out);
		} else {
			num = input & 0x7f;
			num = (num << 8) | getc(in);
			for(i=0;i<num;i++)putc(getc(in),out);
		}
	}
}

int main(int argc, char *argv[]) {
	FILE* in;
	FILE* out;
	if(argc != 4) {
		fprintf(stderr, "Usage: %s input output c/d\n", argc>0?argv[0]:"compress");
		return 1;
	}
	in=fopen(argv[1], "rb");
	if(in==NULL) {
		fputs("input file open error\n", stderr);
		return 1;
	}
	out=fopen(argv[2], "wb");
	if(out==NULL) {
		fputs("output file open error\n", stderr);
		fclose(out);
		return 0;
	}
	if(argv[3][0]=='d') {
		decompress(in, out);
	} else {
		compress(in, out);
	}
	fclose(in);
	fclose(out);
	return 0;
}
