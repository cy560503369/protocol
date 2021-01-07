#ifndef PROTOCOL103_H
#define PROTOCOL103_H

#define COMB_CTRL(PRM,FCB,FCV,FUN) ((((PRM)&0x1)<<6) | (((FCB)&0x1)<<5)|(((FCV)&0x1)<<4)|((FUN)&0xf))
#define FETCH_ACD(ctrl)	(((ctrl)>>5)&0x1)
#define FETCH_FCB(ctrl)	(((ctrl)>>5)&0x1)
#define FETCH_FUN(ctrl)	((ctrl)&0xf)

#define MAX_TRY_COUNT 3
#define	FIXED_FRAME_START_CHAR 0x10
#define	UNFIXED_FRAME_START_CHAR 0x68
#define FIXED_FRAME_TYPE	1
#define UNFIXED_FRAME_TYPE	2
#define MAX_FRAME_LEN	255

typedef enum
{
	C_RCU_NA_3 = 0,//复位通信单元FUNTYPE_RESET_COMM
	C_DR_NA_3 = 3,//传送数据，不定长FUNTYPE_DATA_TRAN
	C_DRN_NA_3 = 4,//传送数据(无回答)，不定长FUNTYPE_DATA_TRAN_NO_RESP
 	C_RFB_NA_3 = 7,//复位帧计数位FUNTYPE_RESET_FRAME_CNT
	C_RLK_NA_3 = 9,//请求链路状态FUNTYPE_REQ_LINK_STAT
	C_PL1_NA_3 = 10,//召唤一级链路数据FUNTYPE_REQ_DATA_LEVEL_1
	C_PL2_NA_3 = 11,//召唤二级链路数据FUNTYPE_REQ_DATA_LEVEL_2
	M_CON_NA_3= 0,//确认FUNTYPE_CNFM_RESP
	M_BY_NA_3= 1,//链路忙\未收到报文FUNTYPE_LINk_BUSY
	M_DR_NA_3= 8,//以数据响应请求帧，不定长FUNTYPE_DATA_RESP
	M_NV_NA_3= 9,//无所召唤的数据FUNTYPE_NO_DATA
	M_LKR_NA_3= 11,//以链路状态或访问请求回答请求帧FUNTYPE_LINK_STAT_RESP
	M_LKSNW_NA_3= 14,//链路服务未工作
	M_LKSND_NA_3= 15,//链路服务未完成
}Tprtcl103_fun_type;

typedef enum
{
	C_SYN_TA_3 = 6,//时间同步
	C_IGI_NA_3 = 7,//总查询
	C_GD_NA_3	= 10,//通用分类数据	
	C_GRC_NA_3	= 20,//一般命令
	C_GC_NA_3	= 21,//通用分类命令
	C_ODT_NA_3	= 24,//扰动数据传输的命令
	C_ADT_NA_3	= 25,//扰动数据传输的认可 
	M_TM_TA_3	= 1,//带时标的报文
	M_TMR_TA_3	= 2,//具有相对时间的带时标的报文
	M_MEI_NA_3	= 3,//被测值I
	M_TME_TA_3	= 4,//具有相对时间的带时标的被测值
	M_IRC_NA_3	= 5,//标识
	M_SYN_TA_3	= 6,//时间同步
	M_TGI_NA_3	= 8,//总查询终止
	M_MEII_NA_3	= 9,//被测值II
	M_GD_NA_3	= 10,//通用分类数据
	M_GI_NA_3	= 11,//通用分类标识
	M_LRD_NA_3	= 23,//被记录的扰动表
	M_RTD_NA_3	= 26,//扰动数据传输准备就绪
	M_RTC_NA_3	= 27,//被记录的通道传输准备就绪
	M_RTT_NA_3	= 28,//带标志的状态变位传输准备就绪
	M_TDT_NA_3	= 29,//传送带标志的状态变位
	M_TDN_NA_3	= 30,//传送扰动值
	M_EOT_NA_3	= 31,//传送结束
}Tprtcl103_asdu_type;//asdu类型标识

typedef enum
{
    ASDU01 = 0x01,
    ASDU02 = 0x02,
    ASDU03 = 0x03,
    ASDU04 = 0x04,
    ASDU05 = 0x05,
    ASDU08 = 0x08,
    ASDU09 = 0x09,
    ASDU10 = 0x0a,
    ASDU11 = 0x0b,
    ASDU23 = 0x17,
    ASDU26 = 0x1a,
    ASDU27 = 0x1b,
    ASDU28 = 0x1c,
    ASDU29 = 0x1d,
    ASDU30 = 0x1e,
    ASDU31 = 0x1f,
    ASDU32 = 0x20,
    ASDU33 = 0x21,
    ASDU34 = 0x22,
    ASDU35 = 0x23,
    ASDU36 = 0x24,
    ASDU37 = 0x25,
    ASDU38 = 0x26,
    ASDU39 = 0x27,
    ASDU40 = 0x28,
    ASDU41 = 0x29,
    ASDU42 = 0x2a,
    ASDU43 = 0x2b,
    ASDU44 = 0x2c,
    ASDU45 = 0x2d,
    ASDU46 = 0x2e,
    ASDU47 = 0x2f,
    ASDU48 = 0x30,
    ASDU50 = 0x32,
}ASDU_TYPE;

typedef struct
{
    int fd;
    unsigned char slave_id;
    unsigned char fcb;
    unsigned char fcv;
}Slave_node;

typedef struct
{
	unsigned char cstart;
	unsigned char ctrl;
	unsigned char slave_addr;
	unsigned char crc;
	unsigned char cstop;
}Tprtcl103_fixed_frame;

typedef struct
{
	unsigned char cstart1;
	unsigned char len1;
	unsigned char len2;
	unsigned char cstart2;
	unsigned char ctrl;
	unsigned char slave_addr;
}Tprtcl103_unfixed_frame_head;

/*************************************/
typedef struct
{
	unsigned char Fun;
	unsigned char Inf;
	unsigned char value;   // 0:分，1:合
}Event_data;

typedef struct
{
	unsigned char group;
	unsigned char entry;
	float value;   // 
}Group_data;

typedef enum
{
	EVENT_DATA,
	GROUP_DATA,
}Data_type;
/* 103协议数据存储结构 */
typedef struct
{
	unsigned char slave_addr;    // 从机地址
	Data_type type;
	union
	{
		Event_data event_data;
		Group_data group_data;
	}data;
}Protocol103_data;

/*配置格式*/
typedef struct 
{
	unsigned char Fun;
	unsigned char Inf;
	char id_name[30];	
}State_table;

typedef struct
{
	unsigned char entry;
	char id_name[30];
}Entry_info;

typedef struct 
{
	unsigned char group;  //组号
	int entry_num;
	Entry_info entry_info[20]; //每个组最多读取20个数据
}Message_table;

typedef struct
{
	unsigned char port[30];  // 串口路径
	int device_num;   // 设备个数
	unsigned char device_addr[5];  // 最多支持5台从机
	int state_table_num; // 事件个数
	State_table state_table[30];
	int message_table_num; // 读取几个组的数据
	Message_table message_table[5];
}Config_info;

int protocol103_main(void);
#endif