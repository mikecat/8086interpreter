#include <stdio.h>

const char *compressed_disk=
	"AArrO5Bta2Rvc2ZzgAAAAgADAgEBgAAAAQACAuCAAAABAARAC/AJgAAAAQABEoAAAAEAAQKAAAAP"
	"ACQgICAgICAgICAgIEZBVDEyICAgMcCM2L5YfIoEhMB0CrQOuw+AAAABABfNEEbr8Pr06/1oZWxs"
	"bywgd29ybGQNCoAAAZgABVWq8P//gAAR/QAD8P//gBZr/Q=="
;

/*
	This interpreter is released under the MIT License.

	Copyright (c) 2015 MikeCAT

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/

int base64_char2id(char c) {
	static const char *table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int i;
	for(i=0;table[i]!='\0';i++) {
		if(table[i]==c)return i;
	}
	return -1;
}

int base64_get_next_char(void) {
	static int pos=0;
	for(;;) {
		char input=compressed_disk[pos];
		int id;
		if(input=='\0')return -1;
		pos++;
		id=base64_char2id(input);
		if(id>=0)return id;
	}
}

int base64_get_next_byte(void) {
	static int buffer;
	static int buffer_pos=0;
	int ret=-1;
	int next=base64_get_next_char();
	if(next<0)return -1;
	if(buffer_pos==0) {
		ret=(next<<2)&0xfc;
		if((next=base64_get_next_char())<0)return -1;
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

unsigned char disk[512*18*80*2];

void decompress_disk(void) {
	int input;
	int num,j;
	int out_pos=0;
	while((input=base64_get_next_byte())>=0) {
		if(input & 0x80) {
			num=input&0x7f;
			num=(num<<8)|base64_get_next_byte();
			num=(num<<8)|base64_get_next_byte();
			num=(num<<8)|base64_get_next_byte();
			for(j=0;j<num;j++)disk[out_pos++]=0;
		} else {
			num=input&0x7f;
			num=(num<<8)|base64_get_next_byte();
			for(j=0;j<num;j++)disk[out_pos++]=base64_get_next_byte();
		}
	}
}

int read_disk(unsigned char *out, int head, int cylinder, int sector) {
	if(0<=head && head<2 && 0<=cylinder && cylinder<80 && 1<=sector && sector<=18) {
		unsigned char *begin=disk+512*(18*(2*cylinder+head)+(sector-1));
		int i;
		for(i=0;i<512;i++)out[i]=begin[i];
		return 1;
	} else {
		return 0;
	}
}

#define RAM_SIZE 0xa0000

unsigned char RAM[RAM_SIZE];

unsigned int REGS[12];
unsigned int IP;
unsigned int FLAGS;

#define CF_BIT 0x0001
#define PF_BIT 0x0004
#define AF_BIT 0x0010
#define ZF_BIT 0x0040
#define SF_BIT 0x0080
#define TF_BIT 0x0100
#define IF_BIT 0x0200
#define DF_BIT 0x0400
#define OF_BIT 0x0800

#define AX REGS[0]
#define CX REGS[1]
#define DX REGS[2]
#define BX REGS[3]
#define SP REGS[4]
#define BP REGS[5]
#define SI REGS[6]
#define DI REGS[7]

#define ES REGS[8]
#define CS REGS[9]
#define SS REGS[10]
#define DS REGS[11]

typedef struct {
	enum {
		TYPE_8BIT_MEM,
		TYPE_8BIT_REG,
		TYPE_16BIT_MEM,
		TYPE_16BIT_REG
	} type;
	int index;
} storage_t;

int read_data(storage_t where) {
	int i=where.index;
	int t=where.type;
	int ret=0;
	if(t==TYPE_8BIT_MEM) {
		if(0<=i && i<RAM_SIZE)ret=RAM[i]; else ret=0xff;
	} else if(t==TYPE_8BIT_REG) {
		if(0<=i && i<4)ret=REGS[i] & 0xff;
		else if(4<=i && i<8)ret=(REGS[i-4]>>8) & 0xff;
	} else if(t==TYPE_16BIT_MEM) {
		if(0<=i && i+1<RAM_SIZE)ret=RAM[i]|(RAM[i+1]<<8);
		else if(i==RAM_SIZE-1)ret=RAM[i]|(0xff<<8);
		else ret=0xffff;
	} else if(t==TYPE_16BIT_REG) {
		if(0<=i && i<12)ret=REGS[i] & 0xffff;
	}
	return ret;
}

void write_data(storage_t where,int data) {
	int i=where.index;
	int t=where.type;
	if(t==TYPE_8BIT_MEM) {
		if(0<=i && i<RAM_SIZE)RAM[i]=data;
	} else if(t==TYPE_8BIT_REG) {
		if(0<=i && i<4)REGS[i]=(REGS[i]&0xff00)|(data&0xff);
		else if(4<=i && i<8)REGS[i-4]=(REGS[i-4]&0x00ff)|((data&0xff)<<8);
	} else if(t==TYPE_16BIT_MEM) {
		if(0<=i && i<RAM_SIZE)RAM[i]=data&0xff;
		if(i+1<RAM_SIZE)RAM[i+1]=(data>>8)&0xff;
	} else if(t==TYPE_16BIT_REG) {
		if(0<=i && i<12)REGS[i]=data&0xffff;
	}
}

int fetch_inst(int offset) {
	storage_t pos;
	pos.type=TYPE_8BIT_MEM;
	pos.index=((CS<<4)+((IP+offset)&0xffff))&0xfffff;
	return read_data(pos);
}

int read_stack(int offset) {
	storage_t pos;
	pos.type=TYPE_16BIT_MEM;
	pos.index=((SS<<4)+((SP+offset)&0xffff))&0xfffff;
	return read_data(pos);
}

void write_stack(int offset,int data) {
	storage_t pos;
	pos.type=TYPE_16BIT_MEM;
	pos.index=((SS<<4)+((SP+offset)&0xffff))&0xfffff;
	write_data(pos,data);
}

int run_in(int port,int is_word) {
	printf("unimplemented IN port=0x%04X is_word=%d at CS=%04X IP=%04X\n",port,is_word,CS,IP);
	return 0;
}

int run_out(int port,int is_word) {
	printf("unimplemented OUT port=0x%04X is_word=%d at CS=%04X IP=%04X\n",port,is_word,CS,IP);
	return 0;
}

int run_int(int type) {
	int int_cs,int_ip;
	storage_t reg;
	reg.type=TYPE_16BIT_MEM;
	reg.index=4*type;
	int_ip=read_data(reg);
	reg.index+=2;
	int_cs=read_data(reg);
	if(int_ip!=0 || int_cs!=0) {
		write_stack(-2,FLAGS);
		write_stack(-4,CS);
		write_stack(-6,IP);
		SP=(SP-6)&0xffff;
		CS=int_cs;
		IP=int_ip;
		return 1;
	} else {
		int service;
		if(type==0x00) {
			printf("#DE (int 0x00) detected at CS=%04X, IP=%04X\n",CS,IP);
			return 0;
		} else if(type==0x06) {
			printf("#UD (int 0x06) detected at CS=%04X, IP=%04X\n",CS,IP);
			return 0;
		} else if(type==0x10) {
			reg.type=TYPE_8BIT_REG;
			reg.index=4;
			service=read_data(reg);
			if(service==0x0e) { /* write a character */
				static int is_prev_cr=0;
				int data;
				reg.index=0;
				data=read_data(reg);
				if(data==0x0d) {
					putchar('\n');
					is_prev_cr=1;
				} else if(data==0x0a) {
					if(!is_prev_cr)putchar('\n');
					is_prev_cr=0;
				} else {
					putchar(data);
					is_prev_cr=0;
				}
				return 1;
			}
		} else if(type==0x11) { /* equipment determination */
			/* see HTTP stanislavs.org/helppc/int_11.html */
			AX=0x0131;
			return 1;
		} else if(type==0x12) { /* get low memory size */
			AX=RAM_SIZE/1024-1;
			return 1;
		} else if(type==0x13) {
			reg.type=TYPE_8BIT_REG;
			reg.index=4;
			service=read_data(reg);
			reg.index=2;
			if(read_data(reg)!=0x00) {
				/* invalid disk number*/
				reg.index=4;
				write_data(reg,0x01);
				FLAGS|=CF_BIT;
				return 1;
			} else {
				if(service==0x00) { /* reset disk */
					FLAGS&=~CF_BIT;
					return 1;
				} else if(service==0x02) { /* read disk */
					int sector_num,cylinder_idx,sector_idx,head_idx;
					reg.index=0;
					sector_num=read_data(reg);
					reg.index=5;
					cylinder_idx=read_data(reg);
					reg.index=1;
					cylinder_idx|=(read_data(reg)&0xc0)<<2;
					sector_idx=read_data(reg)&0x3f;
					reg.index=6;
					head_idx=read_data(reg);
					if(sector_num!=1) {
						/* multi-secctor access is not supported */
						reg.index=4;
						write_data(reg,0x01);
						FLAGS|=CF_BIT;
					} else {
						unsigned char buf[512];
						if(read_disk(buf,head_idx,cylinder_idx,sector_idx)) {
							int i;
							reg.type=TYPE_8BIT_MEM;
							for(i=0;i<512;i++) {
								reg.index=(ES<<4)+((BX+i)&0xffff);
								write_data(reg,buf[i]);
							}
							reg.type=TYPE_8BIT_REG;
							reg.index=4;
							write_data(reg,0x00);
							FLAGS&=~CF_BIT;
						} else {
							reg.index=4;
							write_data(reg,0x04);
							FLAGS|=CF_BIT;
						}
					}
					return 1;
				}
			}
		} else if(type==0x16) {
			reg.type=TYPE_8BIT_REG;
			reg.index=4;
			service=read_data(reg);
			if(service==0x00) { /* read a char */
				/* scan code not supported (return save value as the ASCII code) */
				int input=getchar()&0xff;
				write_data(reg,input);
				reg.index=0;
				write_data(reg,input);
				return 1;
			} else if(service==0x01) { /* check the buffer */
				/* not supported well */
				int input=getchar(); /* assume that there are some characters */
				ungetc(input,stdin);
				write_data(reg,input);
				reg.index=0;
				write_data(reg,input);
				FLAGS&=~ZF_BIT; /* return that there are some characters */
				return 1;
			}
		}
		printf("unimplemented INT type=0x%02X at CS=%04X, IP=%04X\n",type,CS,IP);
		return 0;
	}
}

void vm_init(void) {
	read_disk(&RAM[0x7c00],0,0,1);
	AX=0xaa55;SP=0x7c00;
	ES=CS=SS=DS=0x0000;
	IP=0x7c00;
	FLAGS=2;
}

typedef int (*p_calc_func)(int,int,int);

int mov_func(int src,int dest,int is_word) {
	(void)dest;
	(void)is_word;
	return src;
}

int get_signed_value(int value,int is_word) {
	if(is_word && (value&0x8000)!=0) {
		return -(((~value)+1)&0xffff);
	} else if(!is_word && (value&0x80)!=0) {
		return -(((~value)+1)&0xff);
	} else {
		return value;
	}
}

int cut_value(int value,int is_word) {
	if(is_word) {
		return value&0xffff;
	} else {
		return value&0xff;
	}
}

void set_sf(int value,int is_word) {
	if((is_word && (value&0x8000)!=0) || (!is_word && (value&0x80)!=0))FLAGS|=SF_BIT; else FLAGS&=~SF_BIT;
}

void set_zf(int value) {
	if(value==0)FLAGS|=ZF_BIT; else FLAGS&=~ZF_BIT;
}

void set_pf(int value) {
	int count=value&0xff;
	count=((count&0xaa)>>1)+(count&0x55);
	count=((count&0xcc)>>2)+(count&0x33);
	count=((count&0xf0)>>4)+(count&0x0f);
	if(count%2==0)FLAGS|=PF_BIT; else FLAGS&=~PF_BIT;
}

int addition_func(int src,int dest,int is_word,int do_set_cf,int do_add_carry) {
	int ret=dest+src;
	int cut_ret;
	int ret2=get_signed_value(dest,is_word)+get_signed_value(src,is_word);
	if(do_add_carry && (FLAGS&CF_BIT)!=0) {
		ret++;
		ret2++;
	}
	cut_ret=cut_value(ret,is_word);
	if((is_word && (ret2<-0x8000 || 0x8000<=ret2)) || (!is_word && (ret2<-0x80 || 0x80<=ret2))) {
		FLAGS|=OF_BIT;
	} else {
		FLAGS&=~OF_BIT;
	}
	set_sf(cut_ret,is_word);
	set_zf(cut_ret);
	if((dest&0xf)+(src&0xf)>0xf)FLAGS|=AF_BIT; else FLAGS&=~AF_BIT;
	set_pf(ret);
	if(do_set_cf) {
		if((is_word && ret>0xffff) || (!is_word && ret>0xff))FLAGS|=CF_BIT; else FLAGS&=~CF_BIT;
	}
	return cut_ret;
}

int subtract_func(int src,int dest,int is_word,int do_set_cf,int do_sub_carry) {
	int src_cmp,ret,cut_ret,ret2;
	if(is_word)src_cmp=((~src)&0xffff)+1; else src_cmp=((~src)&0xff)+1;
	ret=dest+src_cmp;
	ret2=get_signed_value(dest,is_word)-get_signed_value(src,is_word);
	if(do_sub_carry && (FLAGS&CF_BIT)!=0) {
		ret--;
		ret2--;
	}
	cut_ret=cut_value(ret,is_word);
	if((is_word && (ret2<-0x8000 || 0x8000<=ret2)) || (!is_word && (ret2<-0x80 || 0x80<=ret2))) {
		FLAGS|=OF_BIT;
	} else {
		FLAGS&=~OF_BIT;
	}
	set_sf(cut_ret,is_word);
	set_zf(cut_ret);
	if((dest&0xf)+(((~src)&0xf)+1)>0xf)FLAGS&=~AF_BIT; else FLAGS|=AF_BIT;
	set_pf(ret);
	if(do_set_cf) {
		if((is_word && ret>0xffff) || (!is_word && ret>0xff))FLAGS&=~CF_BIT; else FLAGS|=CF_BIT;
	}
	return cut_ret;
}

int add_func(int src,int dest,int is_word) {
	return addition_func(src,dest,is_word,1,0);
}

int or_func(int src,int dest,int is_word) {
	int ret=src|dest;
	(void)is_word;
	FLAGS&=~(OF_BIT | CF_BIT);
	set_sf(ret,is_word);
	set_zf(ret);
	set_pf(ret);
	return ret;
}

int adc_func(int src,int dest,int is_word) {
	return addition_func(src,dest,is_word,1,1);
}

int ssb_func(int src,int dest,int is_word) {
	return subtract_func(src,dest,is_word,1,1);
}

int and_func(int src,int dest,int is_word) {
	int ret=src&dest;
	(void)is_word;
	FLAGS&=~(OF_BIT | CF_BIT);
	set_sf(ret,is_word);
	set_zf(ret);
	set_pf(ret);
	return ret;
}

int sub_func(int src,int dest,int is_word) {
	return subtract_func(src,dest,is_word,1,0);
}

int sub_func_for_scas(int src,int dest,int is_word) {
	return subtract_func(dest,src,is_word,1,0);
}

int xor_func(int src,int dest,int is_word) {
	int ret=src^dest;
	(void)is_word;
	FLAGS&=~(OF_BIT | CF_BIT);
	set_sf(ret,is_word);
	set_zf(ret);
	set_pf(ret);
	return ret;
}

int not_func(int src,int dest,int is_word) {
	(void)src;
	return cut_value(~dest,is_word);
}

int neg_func(int src,int dest,int is_word) {
	(void)src;
	return subtract_func(dest,0,is_word,1,0);
}

int rol_func(int src,int dest,int is_word) {
	int ret=dest;
	int last_value=0;
	int i;
	if(src!=0) {
		for(i=0;i<src;i++) {
			if(is_word) {
				ret=((ret<<1)|(ret>>15))&0xffff;
			} else {
				ret=((ret<<1)|(ret>>7))&0xff;
			}
			last_value=ret&1;
		}
		if(src==1) {
			if((is_word && (ret&0x8000)!=0) || (!is_word && (ret&0x80)!=0)) {
				if(last_value)FLAGS&=~OF_BIT; else FLAGS|=OF_BIT;
			} else {
				if(last_value)FLAGS|=OF_BIT; else FLAGS&=~OF_BIT;
			}
		}
		if(last_value)FLAGS|=CF_BIT; else FLAGS&=~CF_BIT;
	}
	return ret;
}

int ror_func(int src,int dest,int is_word) {
	int ret=dest;
	int last_value=0;
	int i;
	if(src!=0) {
		for(i=0;i<src;i++) {
			last_value=ret&1;
			if(is_word) {
				ret=((ret>>1)|(ret<<15))&0xffff;
			} else {
				ret=((ret>>1)|(ret<<7))&0xff;
			}
		}
		if(src==1) {
			if((is_word && ((ret&0xc000)==0xc000 || (ret&0xc000)==0x0000)) ||
			(!is_word && ((ret&0xc0)==0xc0 || (ret&0xc0)==0x00))) {
				FLAGS&=~OF_BIT;
			} else {
				FLAGS|=OF_BIT;
			}
		}
		if(last_value)FLAGS|=CF_BIT; else FLAGS&=~CF_BIT;
	}
	return ret;
}

int rcl_func(int src,int dest,int is_word) {
	int ret=dest;
	int last_value;
	int i;
	if(FLAGS&CF_BIT)last_value=1; else last_value=0;
	if(src!=0) {
		for(i=0;i<src;i++) {
			if(is_word) {
				ret=(ret<<1)|last_value;
				last_value=(ret>>16)&1;
				ret&=0xffff;
			} else {
				ret=(ret<<1)|last_value;
				last_value=(ret>>8)&1;
				ret&=0xff;
			}
		}
		if(src==1) {
			if((is_word && (ret&0x8000)!=0) || (!is_word && (ret&0x80)!=0)) {
				if(last_value)FLAGS&=~OF_BIT; else FLAGS|=OF_BIT;
			} else {
				if(last_value)FLAGS|=OF_BIT; else FLAGS&=~OF_BIT;
			}
		}
		if(last_value)FLAGS|=CF_BIT; else FLAGS&=~CF_BIT;
	}
	return ret;
}

int rcr_func(int src,int dest,int is_word) {
	int ret=dest;
	int last_value;
	int i;
	if(FLAGS&CF_BIT)last_value=1; else last_value=0;
	if(src!=0) {
		for(i=0;i<src;i++) {
			int next_last_value=ret&1;
			if(is_word) {
				ret=((ret>>1)|(last_value<<15))&0xffff;
			} else {
				ret=((ret>>1)|(last_value<<7))&0xff;
			}
			last_value=next_last_value;
		}
		if(src==1) {
			if((is_word && ((ret&0xc000)==0xc000 || (ret&0xc000)==0x0000)) ||
			(!is_word && ((ret&0xc0)==0xc0 || (ret&0xc0)==0x00))) {
				FLAGS&=~OF_BIT;
			} else {
				FLAGS|=OF_BIT;
			}
		}
		if(last_value)FLAGS|=CF_BIT; else FLAGS&=~CF_BIT;
	}
	return ret;
}

int shl_func(int src,int dest,int is_word) {
	int ret=dest<<(src&0x1f);
	int cut_ret=cut_value(ret,is_word);
	if(src!=0) {
		if(src==1) {
			if((is_word && ((src&0xc000) == 0xc000 || (src&0xc000) == 0x0000)) ||
			(!is_word && ((src&0xc0) == 0xc0 || (src&0x0) == 0x00)))FLAGS&=~OF_BIT;
			else FLAGS|=OF_BIT;
		}
		set_sf(cut_ret,is_word);
		set_zf(cut_ret);
		set_pf(cut_ret);
	}
	return cut_ret;
}

int shr_func(int src,int dest,int is_word) {
	int ret=dest>>(src&0x1f);
	if(src!=0) {
		if(src==1) {
			if((is_word && (src&0x8000)!=0) || (!is_word && (src&0x80)!=0))FLAGS|=OF_BIT; else FLAGS&=~OF_BIT;
		}
		if((dest>>((src&0x1f)-1))&1)FLAGS|=CF_BIT; else FLAGS&=~CF_BIT;
		set_sf(ret,is_word);
		set_zf(ret);
		set_pf(ret);
	}
	return ret;
}

int sar_func(int src,int dest,int is_word) {
	int ret=dest;
	int i;
	int last_data=0;
	for(i=0;i<(src&0x1f);i++) {
		last_data=ret&1;
		ret>>=1;
		if(is_word) {
			if(dest&0x8000)ret|=0x8000;
		} else {
			if(dest&0x80)ret|=0x80;
		}
	}
	if(src!=0) {
		if(src==1)FLAGS&=~OF_BIT;
		if(last_data)FLAGS|=CF_BIT; else FLAGS&=~CF_BIT;
		set_sf(ret,is_word);
		set_zf(ret);
		set_pf(ret);
	}
	return ret;
}

int inc_func(int src,int dest,int is_word) {
	(void)src;
	return addition_func(1,dest,is_word,0,0);
}

int dec_func(int src,int dest,int is_word) {
	(void)src;
	return subtract_func(1,dest,is_word,0,0);
}

/* segment<0 -> default, segment>=4 -> no segment(for LEA) */
storage_t fetch_mod_rm(int *size,int offset,int segment,int w) {
	storage_t ret;
	int mod_rm=fetch_inst(offset);
	int mod=(mod_rm>>6)&3;
	int rm=mod_rm&7;
	int disp=0;
	if(mod==0) {
		if(rm==6) {
			*size=3;
			disp=fetch_inst(offset+1)|(fetch_inst(offset+2)<<8);
		} else {
			*size=1;
			disp=0;
		}
	} else if(mod==1) {
		*size=2;
		disp=fetch_inst(offset+1);
		if(disp&0x80)disp|=0xff00;
	} else if(mod==2) {
		*size=3;
		disp=fetch_inst(offset+1)|(fetch_inst(offset+2)<<8);
	} else {
		*size=1;
		if(w)ret.type=TYPE_16BIT_REG; else ret.type=TYPE_8BIT_REG;
		ret.index=rm;
		return ret;
	}
	if(w)ret.type=TYPE_16BIT_MEM; else ret.type=TYPE_8BIT_MEM;
	if(rm==0)ret.index=BX+SI+disp;
	else if(rm==1)ret.index=BX+DI+disp;
	else if(rm==2)ret.index=BP+SI+disp;
	else if(rm==3)ret.index=BP+DI+disp;
	else if(rm==4)ret.index=SI+disp;
	else if(rm==5)ret.index=DI+disp;
	else if(rm==6) {
		if(mod==0)ret.index=disp; else ret.index=BP+disp;
	} else ret.index=BX+disp;
	ret.index&=0xffff;
	if(0<=segment && segment<4) {
		ret.index+=(REGS[8+segment]<<4);
	} else if(segment<0) {
		if(rm==2 || rm==3 || (rm==6 && mod!=0)) {
			ret.index+=(SS<<4);
		} else {
			ret.index+=(DS<<4);
		}
	}
	return ret;
} 

storage_t fetch_reg(int offset,int w) {
	storage_t reg;
	if(w)reg.type=TYPE_16BIT_REG; else reg.type=TYPE_8BIT_REG;
	reg.index=(fetch_inst(offset)>>3)&7;
	return reg;
}

void do_calc_rm_r(p_calc_func func,int segment,int is_test) {
	int inst=fetch_inst(-1);
	int w=inst&1;
	int modrm_size=1;
	storage_t rm=fetch_mod_rm(&modrm_size,0,segment,w);
	storage_t reg=fetch_reg(0,w);
	int calc_res;
	IP=(IP+modrm_size)&0xffff;
	if(inst&2) {
		calc_res=func(read_data(rm),read_data(reg),w);
		if(!is_test)write_data(reg,calc_res);
	} else {
		calc_res=func(read_data(reg),read_data(rm),w);
		if(!is_test)write_data(rm,calc_res);
	}
}

void do_calc_imm_rm(p_calc_func func,int segment,int is_test,int enable_s) {
	int inst=fetch_inst(-1);
	int w=inst&1;
	int modrm_size=1;
	storage_t rm=fetch_mod_rm(&modrm_size,0,segment,w);
	int calc_res;
	int imm;
	IP=(IP+modrm_size)&0xffff;
	imm=fetch_inst(0);
	IP=(IP+1)&0xffff;
	if(w) {
		if(enable_s && (inst&2)!=0) {
			if(imm&0x80)imm|=0xff00;
		} else {
			imm|=fetch_inst(0)<<8;
			IP=(IP+1)&0xffff;
		}
	}
	calc_res=func(imm,read_data(rm),w);
	if(!is_test)write_data(rm,calc_res);
}

void do_calc_imm_acc(p_calc_func func,int is_test) {
	int w=fetch_inst(-1)&1;
	storage_t acc;
	int calc_res;
	int imm;
	if(w)acc.type=TYPE_16BIT_REG; else acc.type=TYPE_8BIT_REG;
	acc.index=0;
	imm=fetch_inst(0);
	IP=(IP+1)&0xffff;
	if(w) {
		imm|=fetch_inst(0)<<8;
		IP=(IP+1)&0xffff;
	}
	calc_res=func(imm,read_data(acc),w);
	if(!is_test)write_data(acc,calc_res);
}

void do_logic(p_calc_func func,int segment) {
	int inst=fetch_inst(-1);
	int w=inst&1;
	int v=inst&2;
	int modrm_size=1;
	int src;
	storage_t rm=fetch_mod_rm(&modrm_size,0,segment,w);
	storage_t reg;
	IP=(IP+modrm_size)&0xffff;
	reg.type=TYPE_8BIT_REG;
	reg.index=1;
	if(v)src=read_data(reg); else src=1;
	write_data(rm,func(src,read_data(rm),w));
}

/* useax_flag&1 -> use AX instead of [SI], useax_flag&2 -> use AX instead of [DI] */
/* ignore ZF if zf_for_continue<0 */
void do_string(p_calc_func func,int rep_flag,int zf_for_continue,int useax_flag,int segment,int is_test,int is_word) {
	int use_segment=segment;
	if(use_segment<0)use_segment=3;
	while(CX!=0 || !rep_flag) {
		/* execute the instruction */
		storage_t src,dest;
		int calc_ret;
		int diff;
		if(is_word) {
			if(useax_flag&1)src.type=TYPE_16BIT_REG; else src.type=TYPE_16BIT_MEM;
			if(useax_flag&2)dest.type=TYPE_16BIT_REG; else dest.type=TYPE_16BIT_MEM;
		} else {
			if(useax_flag&1)src.type=TYPE_8BIT_REG; else src.type=TYPE_8BIT_MEM;
			if(useax_flag&2)dest.type=TYPE_8BIT_REG; else dest.type=TYPE_8BIT_MEM;
		}
		if(useax_flag&1)src.index=0; else src.index=(REGS[8+use_segment]<<4)+SI;
		if(useax_flag&2)dest.index=0; else dest.index=(ES<<4)+DI;
		calc_ret=func(read_data(src),read_data(dest),is_word);
		if(!is_test)write_data(dest,calc_ret);
		/* move the pointer */
		if(is_word)diff=2; else diff=1;
		if(FLAGS&DF_BIT) {
			if((useax_flag&1)==0)SI=(SI-diff)&0xffff;
			if((useax_flag&2)==0)DI=(DI-diff)&0xffff;
		} else {
			if((useax_flag&1)==0)SI=(SI+diff)&0xffff;
			if((useax_flag&2)==0)DI=(DI+diff)&0xffff;
		}
		if(!rep_flag)break;
		/* check the condition */
		CX=(CX-1)&0xffff;
		if(CX==0)break;
		if(zf_for_continue>=0 && ((zf_for_continue && (FLAGS&ZF_BIT)==0) || (!zf_for_continue && (FLAGS&ZF_BIT)!=0)))break;
	}
}

void do_jcc(int condition) {
	int disp=fetch_inst(0);
	IP=(IP+1)&0xffff;
	if(disp&0x80)disp|=0xff00;
	if(condition)IP=(IP+disp)&0xffff;
}

void print_unimplemented(const char *name,int print_second_byte) {
	if(name==NULL) {
		if(print_second_byte) {
			printf("unimplemented instruction %02X /%d at CS=%04X, IP=%04X\n",fetch_inst(-1),(fetch_inst(0)>>3)&7,CS,(IP-1)&0xffff);
		} else {
			printf("unimplemented instruction %02X at CS=%04X, IP=%04X\n",fetch_inst(-1),CS,(IP-1)&0xffff);
		}
	} else {
		if(print_second_byte) {
			printf("unimplemented instruction %s(%02X %02X) at CS=%04X, IP=%04X\n",name,fetch_inst(-1),fetch_inst(0),CS,(IP-1)&0xffff);
		} else {
			printf("unimplemented instruction %s(%02X) at CS=%04X, IP=%04X\n",name,fetch_inst(-1),CS,(IP-1)&0xffff);
		}
	}
}

/* based on http://www.datasheetarchive.com/dlmain/Datasheets-14/DSA-279540.pdf */

int vm_step(void) {
	int segment=-1;
	int rep_flag=0;
	int rep_zf_for_continue=0;
	int inst=fetch_inst(0);
	while((inst&0xe7)==0x26 || (inst&0xfe)==0xf2) {
		if((inst&0xe7)==0x26) { /* segment override prefix */
			segment=(inst>>3)&3;
		} else { /* REP */
			rep_flag=1;
			rep_zf_for_continue=inst&1;
		}
		IP=(IP+1)&0xffff;
		inst=fetch_inst(0);
	}
	IP=(IP+1)&0xffff;
	if((inst&0xfc)==0x88) { /* MOV Register/Memory to/from Register */
		do_calc_rm_r(mov_func,segment,0);
	} else if((inst&0xfe)==0xc6) { /* MOV Immediate to Register/Memory */
		if(((fetch_inst(0)>>3)&7)==0) {
			do_calc_imm_rm(mov_func,segment,0,0);
		} else {
			print_unimplemented(NULL,1);
			return 0;
		}
	} else if((inst&0xf0)==0xb0) { /* MOV Immediate to Register */
		storage_t reg;
		int imm=fetch_inst(0);
		reg.index=inst&7;
		IP=(IP+1)&0xffff;
		if(inst&0x08) {
			reg.type=TYPE_16BIT_REG;
			imm|=fetch_inst(0)<<8;
			IP=(IP+1)&0xffff;
		} else {
			reg.type=TYPE_8BIT_REG;
		}
		write_data(reg,imm);
	} else if((inst&0xfc)==0xa0) {
		storage_t mem;
		storage_t acc;
		mem.index=fetch_inst(0)|(fetch_inst(1)<<8);
		acc.index=0;
		if(0<=segment && segment<4) {
			mem.index+=REGS[8+segment]<<4;
		} else {
			mem.index+=DS<<4;
		}
		IP=(IP+2)&0xffff;
		if(inst&0x01) {
			mem.type=TYPE_16BIT_MEM;
			acc.type=TYPE_16BIT_REG;
		} else {
			mem.type=TYPE_8BIT_MEM;
			acc.type=TYPE_8BIT_REG;
		}
		if((inst&0xfe)==0xa0) { /* MOV Memory to Accumulator */
			write_data(acc,read_data(mem));
		} else { /* MOV Accumulator to Memory */
			write_data(mem,read_data(acc));
		}
	} else if((inst&0xfd)==0x8c) {
		if(((fetch_inst(0)>>5)&1)==0) {
			int modrm_size=1;
			storage_t mem=fetch_mod_rm(&modrm_size,0,segment,1);
			storage_t reg;
			reg.type=TYPE_16BIT_REG;
			reg.index=8+((fetch_inst(0)>>3)&3);
			IP=(IP+modrm_size)&0xffff;
			if(inst==0x8e) { /* MOV Register/Memory to Segment Register */
				write_data(reg,read_data(mem));
			} else { /* MOV Segment Register to Register/Memory */
				write_data(mem,read_data(reg));
			}
		} else {
			print_unimplemented(NULL,1);
			return 0;
		}
	} else if((inst&0xf8)==0x50) { /* PUSH Register */
		storage_t mem;
		mem.type=TYPE_16BIT_REG;
		mem.index=inst&7;
		SP=(SP-2)&0xffff;
		write_stack(0,read_data(mem));
	} else if((inst&0xe7)==0x06) { /* PUSH Segment Register */
		storage_t mem;
		mem.type=TYPE_16BIT_REG;
		mem.index=8+((inst>>3)&3);
		SP=(SP-2)&0xffff;
		write_stack(0,read_data(mem));
	} else if(inst==0x8f) { /* POP Register/Memory */
		if(((fetch_inst(0)>>3)&7)==0) {
			int modrm_size=1;
			storage_t mem=fetch_mod_rm(&modrm_size,0,segment,1);
			write_data(mem,read_stack(0));
			SP=(SP+2)&0xffff;
			IP=(IP+modrm_size)&0xffff;
		} else {
			print_unimplemented(NULL,1);
			return 0;
		}
	} else if((inst&0xf8)==0x58) { /* POP Register */
		storage_t mem;
		mem.type=TYPE_16BIT_REG;
		mem.index=inst&7;
		write_data(mem,read_stack(0));
		SP=(SP+2)&0xffff;
	} else if((inst&0xe7)==0x07) { /* POP Segment Register */
		storage_t mem;
		mem.type=TYPE_16BIT_REG;
		mem.index=8+((inst>>3)&3);
		write_data(mem,read_stack(0));
		SP=(SP+2)&0xffff;
	} else if((inst&0xfe)==0x86) { /* XCHG Register/Memory with Register */
		int modrm_size=1;
		storage_t rm=fetch_mod_rm(&modrm_size,0,segment,inst&1);
		storage_t reg=fetch_reg(0,inst&1);
		int temp;
		IP=(IP+modrm_size)&0xffff;
		temp=read_data(rm);
		write_data(rm,read_data(reg));
		write_data(reg,temp);
	} else if((inst&0xf8)==0x90) { /* XCHG Register with Accumulator */
		storage_t acc;
		storage_t reg;
		int temp;
		acc.type=reg.type=TYPE_16BIT_REG;
		acc.index=0;
		reg.index=inst&7;
		temp=read_data(acc);
		write_data(acc,read_data(reg));
		write_data(reg,temp);
	} else if((inst&0xfe)==0xe4) { /* IN Fixed Port */
		int port=fetch_inst(0);
		int w=inst&1;
		IP=(IP+1)&0xffff;
		return run_in(port,w);
	} else if((inst&0xfe)==0xec) { /* IN Variable Port */
		return run_in(DX,inst&1);
	} else if((inst&0xfe)==0xe6) { /* OUT Fixed Port */
		int port=fetch_inst(0);
		int w=inst&1;
		IP=(IP+1)&0xffff;
		return run_out(port,w);
	} else if((inst&0xfe)==0xee) { /* OUT Variable Port */
		return run_out(DX,inst&1);
	} else if(inst==0xd7) { /* XLAT */
		storage_t mem;
		storage_t reg;
		int use_segment=segment;
		if(use_segment<0)use_segment=3;
		reg.type=TYPE_8BIT_REG;
		reg.index=0;
		mem.type=TYPE_8BIT_MEM;
		mem.index=(REGS[8+use_segment]<<4)+((BX+read_data(reg))&0xffff);
		write_data(reg,read_data(mem));
	} else if(inst==0x8d) { /* LEA */
		int modrm_size=1;
		storage_t rm=fetch_mod_rm(&modrm_size,0,4,0);
		storage_t reg=fetch_reg(0,1);
		IP=(IP+modrm_size)&0xffff;
		if(rm.type!=TYPE_8BIT_MEM) {
			return run_int(6);
		} else {
			write_data(reg,rm.index);
		}
	} else if(inst==0xc5) { /* LDS */
		int modrm_size=1;
		storage_t rm=fetch_mod_rm(&modrm_size,0,segment,1);
		storage_t reg=fetch_reg(0,1);
		IP=(IP+modrm_size)&0xffff;
		if(rm.type!=TYPE_16BIT_MEM) {
			return run_int(6);
		} else {
			write_data(reg,read_data(rm));
			rm.index+=2;
			DS=read_data(rm);
		}
	} else if(inst==0xc4) { /* LES */
		int modrm_size=1;
		storage_t rm=fetch_mod_rm(&modrm_size,0,segment,1);
		storage_t reg=fetch_reg(0,1);
		IP=(IP+modrm_size)&0xffff;
		if(rm.type!=TYPE_16BIT_MEM) {
			return run_int(6);
		} else {
			write_data(reg,read_data(rm));
			rm.index+=2;
			ES=read_data(rm);
		}
	} else if(inst==0x9f) { /* LAHF */
		storage_t reg;
		reg.type=TYPE_8BIT_REG;
		reg.index=4;
		write_data(reg,(FLAGS&0xd5)|2);
	} else if(inst==0x9e) { /* SAHF */
		storage_t reg;
		reg.type=TYPE_8BIT_REG;
		reg.index=4;
		FLAGS=(FLAGS&0xff2a)|(read_data(reg)&0xd5);
	} else if(inst==0x9c) { /* PUSHF */
		SP=(SP-2)&0xffff;
		write_stack(0,FLAGS);
	} else if(inst==0x9d) { /* POPF */
		FLAGS=read_stack(0);
		SP=(SP+2)&0xffff;
	} else if((inst&0xfc)==0x00) { /* ADD Reg.Memory with Register to Either */
		do_calc_rm_r(add_func,segment,0);
	} else if((inst&0xfe)==0x04) { /* ADD Immediate to Accumulator */
		do_calc_imm_acc(add_func,0);
	} else if((inst&0xfc)==0x10) { /* ADC Reg.Memory with Register to Either */
		do_calc_rm_r(adc_func,segment,0);
	} else if((inst&0xfe)==0x14) { /* ADC Immediate to Accumulator */
		do_calc_imm_acc(adc_func,0);
	} else if((inst&0xfc)==0x28) { /* SUB Reg.Memory with Register to Either */
		do_calc_rm_r(sub_func,segment,0);
	} else if((inst&0xfe)==0x2c) { /* SUB Immediate to Accumulator */
		do_calc_imm_acc(sub_func,0);
	} else if((inst&0xfc)==0x18) { /* SSB Reg.Memory with Register to Either */
		do_calc_rm_r(ssb_func,segment,0);
	} else if((inst&0xfe)==0x1c) { /* SSB Immediate to Accumulator */
		do_calc_imm_acc(ssb_func,0);
	} else if((inst&0xfc)==0x38) { /* CMP Reg.Memory with Register to Either */
		do_calc_rm_r(sub_func,segment,1);
	} else if((inst&0xfe)==0x3c) { /* CMP Immediate to Accumulator */
		do_calc_imm_acc(sub_func,1);
	} else if((inst&0xf8)==0x40) { /* INC Register */
		storage_t reg;
		reg.type=TYPE_16BIT_REG;
		reg.index=inst&7;
		write_data(reg,inc_func(1,read_data(reg),1));
	} else if(inst==0x37) { /* AAA */
		storage_t reg;
		reg.type=TYPE_8BIT_REG;
		reg.index=0;
		if((read_data(reg)&0x0f)>9 || (FLAGS&AF_BIT)!=0) {
			write_data(reg,(read_data(reg)+6)&0xff);
			reg.index=4;
			write_data(reg,(read_data(reg)+1)&0xff);
			FLAGS|=AF_BIT | CF_BIT;
		} else {
			FLAGS&=~(AF_BIT | CF_BIT);
		}
		reg.index=0;
		write_data(reg,read_data(reg)&0x0f);
	} else if(inst==0x27) { /* BAA */
		int old_al,old_cf;
		storage_t reg;
		reg.type=TYPE_8BIT_REG;
		reg.index=0;
		old_al=read_data(reg);
		old_cf=(FLAGS&CF_BIT);
		FLAGS&=~CF_BIT;
		if((old_al&0x0f)>9 || (FLAGS&AF_BIT)!=0) {
			write_data(reg,(old_al+6)&0xff);
			if(old_cf!=0 || old_al+6>0xff)FLAGS|=CF_BIT;
			FLAGS|=AF_BIT;
		} else {
			FLAGS&=~AF_BIT;
		}
		if(old_al>0x99 || old_cf!=0) {
			write_data(reg,(read_data(reg)+0x60)&0xff);
			FLAGS|=CF_BIT;
		} else {
			FLAGS&=~CF_BIT;
		}
		old_al=read_data(reg);
		set_sf(old_al,0);
		set_zf(old_al);
		set_pf(old_al);
	} else if((inst&0xf8)==0x48) { /* DEC Register */
		storage_t reg;
		reg.type=TYPE_16BIT_REG;
		reg.index=inst&7;
		write_data(reg,dec_func(1,read_data(reg),1));
	} else if(inst==0x3f) { /* AAS */
		storage_t reg;
		reg.type=TYPE_8BIT_REG;
		reg.index=0;
		if((read_data(reg)&0x0f)>9 || (FLAGS&AF_BIT)!=0) {
			write_data(reg,(read_data(reg)-6)&0xff);
			reg.index=4;
			write_data(reg,(read_data(reg)-1)&0xff);
			FLAGS|=AF_BIT | CF_BIT;
		} else {
			FLAGS&=~(AF_BIT | CF_BIT);
		}
		reg.index=0;
		write_data(reg,read_data(reg)&0x0f);
	} else if(inst==0x2f) { /* DAS */
		int old_al,old_cf;
		storage_t reg;
		reg.type=TYPE_8BIT_REG;
		reg.index=0;
		old_al=read_data(reg);
		old_cf=(FLAGS&CF_BIT);
		FLAGS&=~CF_BIT;
		if((old_al&0x0f)>9 || (FLAGS&AF_BIT)!=0) {
			write_data(reg,(old_al-6)&0xff);
			if(old_cf!=0 || old_al+((~6+1)&0xff)<=0xff)FLAGS|=CF_BIT;
			FLAGS|=AF_BIT;
		} else {
			FLAGS&=~AF_BIT;
		}
		if(old_al>0x99 || old_cf!=0) {
			write_data(reg,(read_data(reg)-0x60)&0xff);
			FLAGS|=CF_BIT;
		} else {
			FLAGS&=~CF_BIT;
		}
		old_al=read_data(reg);
		set_sf(old_al,0);
		set_zf(old_al);
		set_pf(old_al);
	} else if(inst==0xd4) { /* AAM */
		int radix=fetch_inst(0);
		IP=(IP+1)&0xffff;
		if(radix==0x0A) {
			storage_t reg;
			int temp;
			reg.type=TYPE_8BIT_REG;
			reg.index=0;
			temp=read_data(reg);
			write_data(reg,temp%10);
			reg.index=4;
			write_data(reg,temp/10);
			temp%=10;
			set_sf(temp,0);
			set_zf(temp);
			set_pf(temp);
		} else {
			print_unimplemented("AAM",1);
			return 0;
		}
	} else if(inst==0xd5) { /* AAD */
		int radix=fetch_inst(0);
		IP=(IP+1)&0xffff;
		if(radix==0x0A) {
			storage_t reg;
			int temp_al,temp_ah;
			reg.type=TYPE_8BIT_REG;
			reg.index=0;
			temp_al=read_data(reg);
			reg.index=4;
			temp_ah=read_data(reg);
			write_data(reg,0);
			reg.index=0;
			temp_al=(temp_al+temp_ah*10)&0xff;
			write_data(reg,temp_al);
			set_sf(temp_al,0);
			set_zf(temp_al);
			set_pf(temp_al);
		} else {
			print_unimplemented("AAD",1);
			return 0;
		}
	} else if(inst==0x98) { /* CBW */
		storage_t reg;
		int value;
		reg.type=TYPE_8BIT_REG;
		reg.index=0;
		value=read_data(reg);
		if(value&0x80)value|=0xff00;
		AX=value;
	} else if(inst==0x99) { /* CWD */
		if(AX&0x8000)DX=0xffff; else DX=0x0000;
	} else if((inst&0xfc)==0xd0) { /* LOGIC */
		int type=(fetch_inst(0)>>3)&7;
		static const p_calc_func func_array[8]={
			rol_func,ror_func,rcl_func,rcr_func,shl_func,shr_func,NULL,sar_func
		};
		if(type==6) {
			print_unimplemented(NULL,1);
			return 0;
		} else {
			do_logic(func_array[type],segment);
		}
	} else if((inst&0xfc)==0x20) { /* AND Reg./Memory and Register to Either */
		do_calc_rm_r(and_func,segment,0);
	} else if((inst&0xfe)==0x24) { /* AND Immediate to Accumulator */
		do_calc_imm_acc(and_func,0);
	} else if((inst&0xfc)==0x84) { /* TEST Reg./Memory and Register to Either */
		do_calc_rm_r(and_func,segment,1);
	} else if((inst&0xfe)==0xa8) { /* TEST Immediate to Accumulator */
		do_calc_imm_acc(and_func,1);
	} else if((inst&0xfc)==0x08) { /* OR Reg./Memory and Register to Either */
		do_calc_rm_r(or_func,segment,0);
	} else if((inst&0xfe)==0x0c) { /* OR Immediate to Accumulator */
		do_calc_imm_acc(or_func,0);
	} else if((inst&0xfc)==0x30) { /* XOR Reg./Memory and Register to Either */
		do_calc_rm_r(xor_func,segment,0);
	} else if((inst&0xfe)==0x34) { /* XOR Immediate to Accumulator */
		do_calc_imm_acc(xor_func,0);
	} else if((inst&0xfc)==0x80) { /* XXX Immediate to Register/Memory */
		int type=(fetch_inst(0)>>3)&7;
		static const p_calc_func func_array[8] = {
			add_func,or_func,adc_func,ssb_func,and_func,sub_func,xor_func,sub_func
		};
		if((inst&0x02)==0 || (type!=4 && type!=1 && type!=6)) {
			do_calc_imm_rm(func_array[type],segment,type==7,type!=4 && type!=1 && type!=6);
		} else {
			print_unimplemented(NULL,1);
			return 0;
		}
	} else if((inst&0xfe)==0xf6) {
		int type=(fetch_inst(0)>>3)&7;
		if(type==0) { /* TEST Immediate Data and Register/Memory */
			do_calc_imm_rm(and_func,segment,1,0);
		} else if(type==2) { /* NOT */
			do_logic(not_func,segment);
		} else if(type==3) { /* NEG */
			do_logic(neg_func,segment);
		} else if(type==4) { /* MUL */
			int w=inst&1;
			int modrm_size=1;
			unsigned int ret;
			storage_t mem=fetch_mod_rm(&modrm_size,0,segment,w);
			IP=(IP+modrm_size)&0xffff;
			if(w) {
				ret=(unsigned int)AX*(unsigned int)read_data(mem);
				DX=(ret>>16)&0xffff;
				AX=ret&0xffff;
				if(DX==0)FLAGS&=~(OF_BIT | CF_BIT); else FLAGS|=OF_BIT | CF_BIT;
			} else {
				storage_t reg;
				reg.type=TYPE_8BIT_REG;
				reg.index=0;
				ret=read_data(reg)*read_data(mem);
				AX=ret&0xffff;
				if((AX&0xff00)==0)FLAGS&=~(OF_BIT | CF_BIT); else FLAGS|=OF_BIT | CF_BIT;
			}
		} else if(type==5) { /* IMUL */
			int w=inst&1;
			int modrm_size=1;
			int ret;
			storage_t mem=fetch_mod_rm(&modrm_size,0,segment,w);
			IP=(IP+modrm_size)&0xffff;
			if(w) {
				ret=get_signed_value(AX,1)*get_signed_value(read_data(mem),1);
				DX=(ret>>16)&0xffff;
				AX=ret&0xffff;
				if(ret&0x8000) {
					if(DX==0xffff)FLAGS&=~(OF_BIT | CF_BIT); else FLAGS|=OF_BIT | CF_BIT;
				} else {
					if(DX==0x0000)FLAGS&=~(OF_BIT | CF_BIT); else FLAGS|=OF_BIT | CF_BIT;
				}
			} else {
				storage_t reg;
				reg.type=TYPE_8BIT_REG;
				reg.index=0;
				ret=get_signed_value(read_data(reg),0)*get_signed_value(read_data(mem),0);
				AX=ret&0xffff;
				if(ret&0x80) {
					if((ret&0xff00)==0xff00)FLAGS&=~(OF_BIT | CF_BIT); else FLAGS|=OF_BIT | CF_BIT;
				} else {
					if((ret&0xff00)==0x0000)FLAGS&=~(OF_BIT | CF_BIT); else FLAGS|=OF_BIT | CF_BIT;
				}
			}
		} else if(type==6) { /* DIV */
			int w=inst&1;
			int modrm_size=1;
			unsigned int ret,ret2;
			storage_t mem=fetch_mod_rm(&modrm_size,0,segment,w);
			IP=(IP+modrm_size)&0xffff;
			if(w) {
				unsigned int target=(((unsigned int)DX)<<16)|(unsigned int)AX;
				unsigned int div_by=(unsigned int)read_data(mem);
				if(div_by==0)return run_int(0);
				ret=target/div_by;
				ret2=target%div_by;
				if(ret>0xffff)return run_int(0);
				DX=ret2&0xffff;
				AX=ret&0xffff;
			} else {
				storage_t reg;
				unsigned int div_by=read_data(mem);
				if(div_by==0)return run_int(0);
				reg.type=TYPE_8BIT_REG;
				ret=AX/read_data(mem);
				ret2=AX%read_data(mem);
				if(ret>0xff)return run_int(0);
				reg.index=0;
				write_data(reg,ret&0xff);
				reg.index=4;
				write_data(reg,ret2%0xff);
			}
		} else if(type==7) { /* IDIV */
			int w=inst&1;
			int modrm_size=1;
			unsigned ret,ret2;
			storage_t mem=fetch_mod_rm(&modrm_size,0,segment,w);
			IP=(IP+modrm_size)&0xffff;
			if(w) {
				unsigned int target=(((unsigned int)DX)<<16)|(unsigned int)AX;
				unsigned int div_by=(unsigned int)read_data(mem);
				int sign_flag=0;
				if(target&0x80000000) {
					sign_flag|=1;
					target=(~target)+1;
				}
				if(div_by&0x8000) {
					sign_flag|=2;
					div_by=(~div_by)+1;
				}
				if(div_by==0)return run_int(0);
				ret=target/div_by;
				ret2=target%div_by;
				if(sign_flag&1)DX=(-ret2)&0xffff; else DX=ret2&0xffff;
				if(sign_flag==0 || sign_flag==3) {
					if(ret>0x7fff)return run_int(0);
					AX=ret&0xffff;
				} else {
					if(ret>0x8000)return run_int(0);
					AX=(-ret)&0xffff;
				}
			} else {
				storage_t reg;
				unsigned int target=(unsigned int)AX;
				unsigned int div_by=(unsigned int)read_data(mem);
				int sign_flag=0;
				if(target&0x8000) {
					sign_flag|=1;
					target=(~target)+1;
				}
				if(div_by&0x80) {
					sign_flag|=2;
					div_by=(~div_by)+1;
				}
				if(div_by==0)return run_int(0);
				ret=target/div_by;
				ret2=target%div_by;
				reg.type=TYPE_8BIT_REG;
				reg.index=4;
				if(sign_flag&1)write_data(reg,(-ret2)&0xff); else write_data(reg,ret2&0xff);
				reg.index=0;
				if(sign_flag==0 || sign_flag==3) {
					if(ret>0x7f)return run_int(0);
					write_data(reg,ret&0xff);
				} else {
					if(ret>0x80)return run_int(0);
					write_data(reg,(-ret)&0xff);
				}
			}
		} else {
			print_unimplemented(NULL,1);
			return 0;
		}
	} else if((inst&0xfe)==0xa4) { /* MOVS */
		do_string(mov_func,rep_flag,-1,0,segment,0,inst&1);
	} else if((inst&0xfe)==0xa6) { /* CMPS */
		do_string(sub_func,rep_flag,rep_zf_for_continue,0,segment,1,inst&1);
	} else if((inst&0xfe)==0xae) { /* SCAS */
		do_string(sub_func_for_scas,rep_flag,rep_zf_for_continue,1,segment,1,inst&1);
	} else if((inst&0xfe)==0xac) { /* LODS */
		do_string(mov_func,rep_flag,-1,2,segment,0,inst&1);
	} else if((inst&0xfe)==0xaa) { /* STOS */
		do_string(mov_func,rep_flag,-1,1,segment,0,inst&1);
	} else if(inst==0xe8) { /* CALL Direct within Segment */
		int offset=fetch_inst(0)|(fetch_inst(1)<<8);
		SP=(SP-2)&0xffff;
		IP=(IP+2)&0xffff;
		write_stack(0,IP);
		IP=(IP+offset)&0xffff;
	} else if(inst==0x9a) { /* CALL Direct Intersegment */
		int offset=fetch_inst(0)|(fetch_inst(1)<<8);
		int seg=fetch_inst(2)|(fetch_inst(3)<<8);
		SP=(SP-4)&0xffff;
		write_stack(0,(IP+4)&0xffff);
		write_stack(2,CS);
		CS=seg;
		IP=offset;
	} else if(inst==0xe9) { /* JMP Direct within Segment */
		int offset=fetch_inst(0)|(fetch_inst(1)<<8);
		IP=(IP+2+offset)&0xffff;
	} else if(inst==0xeb) { /* JMP Direct within Segment-Short */
		int offset=fetch_inst(0);
		if(offset&0x80)offset|=0xff00;
		IP=(IP+1+offset)&0xffff;
	} else if(inst==0xea) { /* JMP Direct Intersegment */
		int offset=fetch_inst(0)|(fetch_inst(1)<<8);
		int seg=fetch_inst(2)|(fetch_inst(3)<<8);
		CS=seg;
		IP=offset;
	} else if((inst&0xfe)==0xfe) {
		int type=(fetch_inst(0)>>3)&7;
		if(type==0) { /* INC Register/Memory */
			do_logic(inc_func,segment);
		} else if(type==1) { /* DEC Register/memory */
			do_logic(dec_func,segment);
		} else if((inst&1)==0) {
			print_unimplemented(NULL,1);
			return 0;
		} else if(type==2) { /* CALL Indirect within Segment */
			int modrm_size=1;
			storage_t mem=fetch_mod_rm(&modrm_size,0,segment,1);
			IP=(IP+modrm_size)&0xffff;
			SP=(SP-2)&0xffff;
			write_stack(0,IP);
			IP=read_data(mem);
		} else if(type==3) { /* CALL Indirect Intersegment */
			int modrm_size=1;
			storage_t mem=fetch_mod_rm(&modrm_size,0,segment,1);
			if(mem.type!=TYPE_16BIT_MEM) {
				print_unimplemented("CALL",1);
				return 0;
			} else {
				IP=(IP+modrm_size)&0xffff;
				SP=(SP-4)&0xffff;
				write_stack(0,IP);
				write_stack(2,CS);
				IP=read_data(mem);
				mem.index+=2;
				CS=read_data(mem);
			}
		} else if(type==4) { /* JMP Indirect within Segment */
			int modrm_size=1;
			storage_t mem=fetch_mod_rm(&modrm_size,0,segment,1);
			IP=read_data(mem);
		} else if(type==5) { /* JMP Indirect Intersegment */
			int modrm_size=1;
			storage_t mem=fetch_mod_rm(&modrm_size,0,segment,1);
			if(mem.type!=TYPE_16BIT_MEM) {
				print_unimplemented("JMP",1);
				return 0;
			} else {
				IP=read_data(mem);
				mem.index+=2;
				CS=read_data(mem);
			}
		} else if(type==6) { /* PUSH Register/Memory */
			int modrm_size=1;
			storage_t mem=fetch_mod_rm(&modrm_size,0,segment,1);
			SP=(SP-2)&0xffff;
			IP=(IP+modrm_size)&0xffff;
			write_stack(0,read_data(mem));
		} else {
			print_unimplemented(NULL,1);
			return 0;
		}
	} else if(inst==0xc3) { /* RET Within Segment */
		IP=read_stack(0);
		SP=(SP+2)&0xffff;
	} else if(inst==0xc2) { /* RET Within Seg Adding Immed to SP */
		int offset=fetch_inst(0)|(fetch_inst(1)<<8);
		IP=read_stack(0);
		SP=(SP+2+offset)&0xffff;
	} else if(inst==0xcb) { /* RET Intersegment */
		IP=read_stack(0);
		CS=read_stack(2);
		SP=(SP+4)&0xffff;
	} else if(inst==0xca) { /* RET Intersegment Adding Immediate to SP */
		int offset=fetch_inst(0)|(fetch_inst(1)<<8);
		IP=read_stack(0);
		CS=read_stack(2);
		SP=(SP+4+offset)&0xffff;
	} else if(inst==0x74) { /* JE/JZ */
		do_jcc(FLAGS & ZF_BIT);
	} else if(inst==0x7c) { /* JL/JNGE */
		do_jcc(((FLAGS & SF_BIT)!=0) != ((FLAGS & OF_BIT)!=0));
	} else if(inst==0x7e) { /* JLE/JNG */
		do_jcc(((FLAGS & ZF_BIT)!=0) || (((FLAGS & SF_BIT)!=0) != ((FLAGS & OF_BIT)!=0)));
	} else if(inst==0x72) { /* JB/JNAE */
		do_jcc(FLAGS & CF_BIT);
	} else if(inst==0x76) { /* JBE/JNA */
		do_jcc(FLAGS & (CF_BIT | ZF_BIT));
	} else if(inst==0x7a) { /* JP/JPE */
		do_jcc(FLAGS & PF_BIT);
	} else if(inst==0x70) { /* JO */
		do_jcc(FLAGS & OF_BIT);
	} else if(inst==0x78) { /* JS */
		do_jcc(FLAGS & SF_BIT);
	} else if(inst==0x75) { /* JNE/JNZ */
		do_jcc((FLAGS & ZF_BIT)==0);
	} else if(inst==0x7d) { /* JNL/JGE */
		do_jcc(((FLAGS & SF_BIT)!=0) == ((FLAGS & OF_BIT)!=0));
	} else if(inst==0x7f) { /* JNLE/JG */
		do_jcc(((FLAGS & ZF_BIT)==0) && (((FLAGS & SF_BIT)!=0) == ((FLAGS & OF_BIT)!=0)));
	} else if(inst==0x73) { /* JNB/JAE */
		do_jcc((FLAGS & CF_BIT)==0);
	} else if(inst==0x77) { /* JNBE/JA */
		do_jcc(((FLAGS & CF_BIT)==0) && ((FLAGS & ZF_BIT)==0));
	} else if(inst==0x7b) { /* JNP/JPO */
		do_jcc((FLAGS & PF_BIT)==0);
	} else if(inst==0x71) { /* JNO */
		do_jcc((FLAGS & OF_BIT)==0);
	} else if(inst==0x79) { /* JNB */
		do_jcc((FLAGS & CF_BIT)==0);
	} else if(inst==0xe2) { /* LOOP */
		CX=(CX-1)&0xffff;
		do_jcc(CX!=0);
	} else if(inst==0xe1) { /* LOOPZ/LOOPE */
		CX=(CX-1)&0xffff;
		do_jcc(CX!=0 && ((FLAGS & ZF_BIT)!=0));
	} else if(inst==0xe0) { /* LOOPNZ/LOOPNE */
		CX=(CX-1)&0xffff;
		do_jcc(CX!=0 && ((FLAGS & ZF_BIT)==0));
	} else if(inst==0xe3) { /* JCXZ */
		do_jcc(CX==0);
	} else if(inst==0xcd) { /* INT Type Specified */
		int type=fetch_inst(0);
		IP=(IP+1)&0xffff;
		return run_int(type);
	} else if(inst==0xcc) { /* INT Type 3 */
		return run_int(3);
	} else if(inst==0xce) { /* INTO */
		if(FLAGS & OF_BIT)return run_int(4);
	} else if(inst==0xcf) { /* IRET */
		IP=read_stack(0);
		CS=read_stack(2);
		FLAGS=read_stack(4);
		SP=(SP+6)&0xffff;
	} else if(inst==0xf8) { /* CLC */
		FLAGS &= ~CF_BIT;
	} else if(inst==0xf5) { /* CMC */
		FLAGS ^= CF_BIT;
	} else if(inst==0xf9) { /* STC */
		FLAGS |= CF_BIT;
	} else if(inst==0xfc) { /* CLD */
		FLAGS &= ~DF_BIT;
	} else if(inst==0xfd) { /* STD */
		FLAGS |= DF_BIT;
	} else if(inst==0xfa) { /* CLI */
		FLAGS &= ~IF_BIT;
	} else if(inst==0xfb) { /* STI */
		FLAGS |= IF_BIT;
	} else if(inst==0xf4) { /* HLT */
		return 0;
	} else if(inst==0x9b) { /* WAIT */
		/* nothing */
	} else if((inst&0xf8)==0xd8) { /* ESC */
		int modrm_size=1;
		fetch_mod_rm(&modrm_size,0,segment,0);
		IP=(IP+modrm_size)&0xffff;
	} else if(inst==0xf0) { /* LOCK */
		/* nothing */
	} else {
		print_unimplemented(NULL,0);
		return 0;
	}
	return 1;
}

int main(int argc, char *argv[]) {
	if(argc!=2) {
		decompress_disk();
	} else {
		FILE* fp=fopen(argv[1],"rb");
		if(fp==NULL) {
			fputs("disk file open error\n",stderr);
			return 1;
		}
		fread(disk,1,sizeof(disk),fp);
		fclose(fp);
	}
	vm_init();
	while(vm_step());
	return 0;
}
