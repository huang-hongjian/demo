#include<stdlib.h>
#include<stdio.h>
#include<string.h>
extern "C" {
#include "extern_fec.h"
}

#define ECC_K 10             //数据块数量
#define ECC_N 14            //数据块和校验块数量总和
#define ECC_C (ECC_N-ECC_K) //校验块数量
#define DATA_LEN 880         //所有数据的总长度，也就是src_data的总长度
#define BLK_SZ (DATA_LEN/ECC_K)//每一块数据的长度
char * buf[ECC_K]={ //存放原始数据信息 
	"0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
	"1111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
	"2222222222222222222222222222222222222222222222222222222222222222222222222222222222222222",
	"3333333333333333333333333333333333333333333333333333333333333333333333333333333333333333",
	"4444444444444444444444444444444444444444444444444444444444444444444444444444444444444444",
	"5555555555555555555555555555555555555555555555555555555555555555555555555555555555555555",
	"6666666666666666666666666666666666666666666666666666666666666666666666666666666666666666",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"8888888888888888888888888888888888888888888888888888888888888888888888888888888888888888",
	"9999999999999999999999999999999999999999999999999999999999999999999999999999999999999999",
	
};
char  buf_src[ECC_K][89]={0};

char  fec[ECC_C][89]={0};
int main(int argc,char **argv)
{
	printf("222222222222222222222222222222");
    int i;

    char* src_data="abcdefghijklmnopqrstuvwx123456987654";
    char data[100] ;                             //存放原始数据信息 
    char cdata[100]={0};                         //存放校验数据信息
    char *src[ECC_K];                            //存放原始数据信息块指针的矩阵
    char *fecs[ECC_C];                           //校验信息块指针的矩阵
	unsigned lost[ECC_N];                        //描述数据块和校验块是否丢失的矩阵
    char *out_recovery[ECC_K];                   
    printf("1111111111111111111");
    //strcpy(data,src_data);   
    for(i=0;i<ECC_K;i++){
        memcpy(buf_src[i],buf[i],BLK_SZ);
	    printf("buf_src[%d]=%s\n",i,buf_src[i]);
	}
    for(i=0;i<ECC_K;i++)                           //填充原始数据
	{
        src[i]=buf_src[i];
		printf("src[%d]=%s\n",i,src[i]);
    }
	for(i=0;i<ECC_C;i++) 
    {
         fecs[i]=fec[i]; 
    }
    
	fec_init_adp(ECC_K,ECC_C,BLK_SZ);
    //printf("src:\t【%s】\n",*src);                      //输出原始数据信息
    printf("ECC_C / BLK_SZ:\t%d / %d\n",ECC_C,BLK_SZ);
 
    //由原始数据得到校验数据
	fec_encode_adp((unsigned char**)src,(unsigned char**)fecs);
    printf("fecs:\t【%s】\n",*fecs);
    printf("================\n");

    //假设数据快d0,d1失效，c1,c2失效，并以校验块c0来恢复丢失的数据包
	//数据包和校验包，任意数据包丢失的包数 <= 校验包数量，都可以恢复原始数据，
	//下列丢失了四个包，两个数据包和两个校验包，都能够恢复回来
	//0表示该块数据丢失，1表示未丢失，鬼知道移动为什么这么定义
    //6个数据包  
    lost[0]=0;
    lost[1]=1;
    lost[2]=0;
    lost[3]=1;
	lost[4]=1;
	lost[5]=1;
	lost[6]=1;
	lost[7]=1;
	lost[8]=1;
	lost[9]=1;
	//4个校验包
	lost[10]=1;
	lost[11]=1;
	lost[12]=0;
	lost[13]=0;
	//lost[14]=1;
	//lost[15]=1;	
	//lost描述的是数据包和校验包是否有丢失的矩阵

	//下面代码是模拟数据丢失操作
    memset(src[0],0,BLK_SZ);
	//memset(src[1],0,BLK_SZ);
	memset(src[2],0,BLK_SZ);
	//memset(src[9],0,BLK_SZ);
    memset(fecs[2],0,BLK_SZ);
	memset(fecs[3],0,BLK_SZ);
	
    //指针指向源数据块，准备恢复数据
    out_recovery[0]=src[0];
    out_recovery[1]=src[1];
    out_recovery[2]=src[2];
    out_recovery[3]=src[3];
	out_recovery[4]=src[4];
	out_recovery[5]=src[5];
    out_recovery[6]=src[6];
    out_recovery[7]=src[7];
	out_recovery[8]=src[8];
	out_recovery[9]=src[9];
	
    //数据恢复操作，并将恢复的数据重新填入到in_recovery中
	fec_decode_adp((unsigned char **)out_recovery, (unsigned char **)fecs, (int*)lost);
	for(i=0;i<ECC_K;i++){
	    printf("out_recovery[%d]=%s\n",i,out_recovery[i]);
    }
    fec_cleanup_adp();
    return 0;
}
