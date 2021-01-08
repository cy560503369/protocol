#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include "cJSON.h"

/* 协议共享内存KEY值定义 */
/* 配置共享内存 */
#define PROTOCOL103_CONFIG_SM_KEY 103
#define PROTOCOL103_CONFIG_LEN 4*1024
#define PROTOCOL104_CONFIG_SM_KEY 104
#define PROTOCOL104_CONFIG_LEN 4*1024
#define PROTOCOL102_CONFIG_SM_KEY 102
#define PROTOCOL102_CONFIG_LEN 4*1024

/* 数据共享内存 */
#define PROTOCOL103_DATA_SM_KEY 203
#define PROTOCOL103_DATA_LEN 4*1024
#define PROTOCOL104_DATA_SM_KEY 204
#define PROTOCOL104_DATA_LEN 4*1024
#define PROTOCOL102_DATA_SM_KEY 202
#define PROTOCOL102_DATA_LEN 4*1024

/*配置共享内存格式*/
typedef struct
{
	pthread_rwlock_t rwlock;
	char update_flag;	// 是否更新配置
	char started;		// 开启标志，如果为1表示开启，其他为关闭
	char config_data[10 * 1024];  // 存放json格式的配置文件
}Protocol_config_sm;

/*数据共享内存格式*/
typedef struct
{
	pthread_rwlock_t rwlock;
	char protocol_data[10 * 1024];  // 存放协议获取到的数据，json格式
}Protocol_data_sm;

void* get_shared_memory(key_t key);
#endif