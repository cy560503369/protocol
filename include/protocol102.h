#ifndef PROTOCOL102_H
#define PROTOCOL102_H

#define	FIX_FRAME_START_CHAR 0x10
#define	VAR_FRAME_START_CHAR 0x68
#define SINGLE_RESPON 0xe5   // 单字符回复帧

#define MAX_FRAME_LENGTH	255

#define COMBINE_CTRL(PRM,FCB,FCV,FUN) ((((PRM)&0x1)<<6) | (((FCB)&0x1)<<5)|(((FCV)&0x1)<<4)|((FUN)&0xf))
#define GET_ACD(ctrl)	(((ctrl)>>5)&0x1)
#define GET_FCB(ctrl)	(((ctrl)>>5)&0x1)
#define GET_FUN(ctrl)	((ctrl)&0xf)

typedef enum
{
	C_RCU_NA_2 = 0, //复位通信单元FUNTYPE_RESET_COMM
	C_DR_NA_3 = 3,//传送数据，不定长FUNTYPE_DATA_TRAN
	C_RLK_NA_3 = 9, //请求链路状态
	C_PL1_NA_2 = 10, // 召唤1级用户数据
	C_PL2_NA_2 = 11, // 召唤2级用户数据
}Tprtcl102_fun_type;

typedef enum
{
	C_RD_DLA_2 = 116,  	// 当前电量
	C_SP_NA_2 = 101,	// 当前单点信息
	C_CI_NR_2 = 120,	// 历史电量
	C_SP_NB_2 = 102,	// 历史单点信息
}Tprtcl102_asdu_type;//asdu类型标识

typedef struct
{
    int fd;
    unsigned short slave_id;
    unsigned char fcb;
    unsigned char fcv;
}Slave_102_node;

typedef struct
{
	unsigned char cstart;
	unsigned char ctrl;
	unsigned char slave_addr_low;
	unsigned char slave_addr_high;
	unsigned char crc;
	unsigned char cstop;
}Tprtcl102_fixed_frame;

typedef struct
{
	unsigned char cstart1;
	unsigned char len1;
	unsigned char len2;
	unsigned char cstart2;
	unsigned char ctrl;
	unsigned char slave_addr_low;
	unsigned char slave_addr_high;
}Tprtcl102_unfixed_frame_head;

/*************************************/
typedef struct
{
	unsigned short slave_addr;
	unsigned char record_id;  // 记录地址
	unsigned char info_id;	 // 信息体地址
	unsigned int data;
}Protocol102_data;

/*配置格式*/
typedef struct
{
	unsigned char record_addr;
	unsigned char info_addr;
	char point_name[30];   // 名称
}Cat_info;

typedef struct
{
	char port[30];  // 串口路径
	int device_num;   // 设备个数
	unsigned short device_addr[5];  // 最多支持5台从机
	unsigned int catch_num;
	Cat_info catch_list[200];
}Config_102_info;

#endif
