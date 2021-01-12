#ifndef PROTOCOL104_H
#define PROTOCOL104_H

typedef struct
{
	unsigned int point_addr;  // 点号
	char point_name[30];   // 名称
}Catch_data;
/* 配置信息结构 */
typedef struct
{
	char ip[20];			// ip地址
	unsigned short port;	// 端口
	int device_num;   // 设备个数
	unsigned char device_addr[5];  // 最多支持5台从机
	unsigned int catch_num;
	Catch_data catch_list[200];		// 获取数据的列表
} Procotol104_config;

/* 数据结构 */
typedef enum
{
	TYPE_INT,
	TYPE_FLOAT
} DATA_TYPE; 

typedef union
{
	unsigned int i_data;
	float f_data;
} VALUE;

typedef struct
{
	unsigned int point_addr;
	DATA_TYPE type;
	VALUE value;
}Procotol104_data;

#endif