#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "protocol.h"
#include "protocol102.h"
#include "uart.h"

Protocol102_data prot102_data[1000] = {0};
int data102_post = 0;

/* 获得微秒时间 */
unsigned long get_now_time()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000); //ms
    //debug("ts.tv_sec %u ts.tv_nsec %u\n",ts.tv_sec,ts.tv_nsec);
    //return (ts.tv_sec * 1000000 + ts.tv_nsec / 1000); //us
}
/**********************************************************************
*	函数名称：unsigned char make_cs(unsigned char *cal_data, unsigned short data_len)	
*	函数功能：计算数据的校验和
**********************************************************************/
unsigned char make_cs(unsigned char *cal_data, unsigned short data_len)
{
	int i;
	unsigned int sum = 0;
	for(i=0; i<data_len; i++)
	{
		sum += cal_data[i];
	}
	return (unsigned char)(sum & 0xff);
}

/*
	组固定长报文
*/
int pack_fix_frame(unsigned char contrl, unsigned short slave_addr, 
                     unsigned char* frame)
{
	if(frame == NULL)
	{
		return -1;
	}

	frame[0] = 0x10;
	frame[1] = contrl;
	frame[2] = (unsigned short)(slave_addr & 0xff);
    frame[3] = (unsigned short)((slave_addr >> 8) & 0xff);
	frame[4] = make_cs(&frame[1], 3);
	frame[5] = 0x16;
	return 0;
}

/*
	组不定长报文
*/
int pack_var_frame(unsigned char contrl, unsigned short slave_addr, 
                        unsigned char* data, unsigned char data_len, 
                        unsigned char* frame)
{
	if(frame == NULL || data == NULL)
	{
		return -1;
	}

	frame[0] = 0x68;
	frame[1] = data_len + 3;
	frame[2] = data_len + 3;
	frame[3] = 0x68;
	frame[4] = contrl;
	frame[5] = (unsigned short)(slave_addr & 0xff);
    frame[6] = (unsigned short)((slave_addr >> 8) & 0xff);
	memcpy(&frame[7], data, data_len);
	frame[7 + data_len] = make_cs(&frame[4], data_len + 3);
	frame[8 + data_len] = 0x16;
	return 0;
}

int receive_frame(Slave_102_node* pslave_node, unsigned char* pframe_buf)
{
    int ret;
    int fd = pslave_node->fd;
    unsigned char* pbuf = pframe_buf;
    unsigned long utime_start = get_now_time();

    while(1)
    {
        ret = serial_receive_data(fd, pbuf, 1);
        if((ret == 1) && (*pbuf == FIX_FRAME_START_CHAR))  // 固定长度帧
        {
            pbuf++;
            ret = serial_receive_data(fd, pbuf, sizeof(Tprtcl102_fixed_frame)-1);
            if(ret < sizeof(Tprtcl102_fixed_frame)-1)
            {
                tcflush(fd, TCIFLUSH);
                return -1;
            }
            return 0;
        }
        else if(ret == 1 && *pbuf == VAR_FRAME_START_CHAR) // 不定长度帧
        {
            pbuf++;
            // 读帧头
            ret = serial_receive_data(fd, pbuf, sizeof(Tprtcl102_unfixed_frame_head)-1);
            if(ret < sizeof(Tprtcl102_unfixed_frame_head)-1)
            {
                tcflush(fd, TCIFLUSH);
                return -1;
            }
            pbuf += sizeof(Tprtcl102_unfixed_frame_head)-1;
            // 读帧头后续内容
            int len = ((Tprtcl102_unfixed_frame_head*)pframe_buf)->len1;
            ret = serial_receive_data(fd, pbuf, len);
            if(ret < len)
            {
                tcflush(fd, TCIFLUSH);
                return -1;
            }
            return 0;
        }
        else if((ret == 1) && (*pbuf == SINGLE_RESPON))  // 单字节回复帧
        {
            return 0;
        }

        else if(ret == 1)   // 非起始字符
        {
        	if((get_now_time() - utime_start) > 10000)    //读10s还没读到起始字符
        	{
        		return -1;
        	}
        	else
        	{
        		continue;
        	}
        }
        else
        {
        	if((get_now_time() - utime_start) > 3000)  //读3秒没读到数据
        	{
        		return -1;
        	}
        	else
        	{
        		continue;
        	}
        }
    }
}

/******************************************************/
int unpack_asdu_frame(char* buff, unsigned short slave_addr)
{
	unsigned char asdu_type = buff[0];
	switch(asdu_type)
	{
		case 0x01:  // 单点信息
        {
            unsigned char data_num = buff[1] & 0x7f;
			int i = 0, off_set = 0;
            for(i = 0; i < data_num; i++)
            {
                prot102_data[data102_post].slave_addr = slave_addr;
                prot102_data[data102_post].record_id = buff[5];
                prot102_data[data102_post].info_id = buff[6 + off_set];
                if(buff[7 + off_set] & 0x01)
                {
                    prot102_data[data102_post].data = 1;
                }
                else
                {
                    prot102_data[data102_post].data = 0;
                }

                off_set += 9;
                data102_post++;
            }
        }
            break;
		case 0x02: // 电量数据
		{
            unsigned char data_num = buff[1] & 0x7f;
			int i = 0, off_set = 0;
            for(i = 0; i < data_num; i++)
            {
                prot102_data[data102_post].slave_addr = slave_addr;
                prot102_data[data102_post].record_id = buff[5];
                prot102_data[data102_post].info_id = buff[6 + off_set];
                memcpy(&prot102_data[data102_post].data, &buff[7 + off_set], 4);

                off_set += 6;
                data102_post++;
            }
		}
			break;

		default:
			break;
	}
	return 0;
}

/* 
    判断应答帧resp_frame是否为命令帧cmd_frame的正确应答
    返回值：-1，不是正确应答；0，是正确应答且acd为0；1，是正确应答且acd为1
 */
int parse_respon_frame(unsigned char* cmd_frame, unsigned char* resp_frame, Slave_102_node* pslave_node)
{
    int resp_frame_ctrl;
    int resp_frame_acd;
    int resp_frame_fun;
    unsigned short cmd_slave_addr,resp_slave_addr;
     
    Tprtcl102_fixed_frame* pfixed_cmd_frame = NULL;
    Tprtcl102_fixed_frame* pfixed_resp_frame = NULL;
    Tprtcl102_unfixed_frame_head* punfixed_cmd_frame = NULL;
    Tprtcl102_unfixed_frame_head* punfixed_resp_frame = NULL;
     
    if((NULL == cmd_frame) || (NULL == resp_frame)
        || ((*cmd_frame != FIX_FRAME_START_CHAR) && (*cmd_frame != VAR_FRAME_START_CHAR))
        || ((*resp_frame != FIX_FRAME_START_CHAR) && (*resp_frame != VAR_FRAME_START_CHAR)))
    {
        return -1;
    }
    
    if(*resp_frame == SINGLE_RESPON)
    {
        return 0;
    }

    if(*cmd_frame == FIX_FRAME_START_CHAR)
    {
        pfixed_cmd_frame = (Tprtcl102_fixed_frame*)cmd_frame;
        cmd_slave_addr = (unsigned short)((pfixed_cmd_frame->slave_addr_high << 8) | pfixed_cmd_frame->slave_addr_low);
    }
    else
    {

        punfixed_cmd_frame = (Tprtcl102_unfixed_frame_head*)cmd_frame;
        cmd_slave_addr = (unsigned short)((punfixed_cmd_frame->slave_addr_high << 8) | punfixed_cmd_frame->slave_addr_low);
    }
    
    if(*resp_frame == FIX_FRAME_START_CHAR)
    {
        pfixed_resp_frame = (Tprtcl102_fixed_frame*)resp_frame;
        /* 校验crc */
        if(make_cs(&pfixed_resp_frame->ctrl, 3) != pfixed_resp_frame->crc)
        {
        	return -1;
        }
        resp_frame_ctrl = pfixed_resp_frame->ctrl;
		resp_slave_addr = (unsigned short)((pfixed_resp_frame->slave_addr_high << 8) | pfixed_resp_frame->slave_addr_low);
    }
    else
    {
        punfixed_resp_frame = (Tprtcl102_unfixed_frame_head*)resp_frame;
        /* 校验crc */
        if(make_cs(&punfixed_resp_frame->ctrl, punfixed_resp_frame->len1) != 
        	*(resp_frame + punfixed_resp_frame->len1 + 4))
        {
        	return -1;
        }
        resp_frame_ctrl = punfixed_resp_frame->ctrl;
		resp_slave_addr = (unsigned short)((punfixed_resp_frame->slave_addr_high << 8) | punfixed_resp_frame->slave_addr_low);
    }

    resp_frame_acd = GET_ACD(resp_frame_ctrl);
	resp_frame_fun = GET_FUN(resp_frame_ctrl);

	if(cmd_slave_addr != resp_slave_addr)
	{
		return -1;
	}

	switch(resp_frame_fun)
	{
		case 0:
		case 1:	
		case 9:
		case 11:
			return resp_frame_acd;
		case 8:
			unpack_asdu_frame((char*)(resp_frame+7), pslave_node->slave_id);
			return resp_frame_acd;
		default:
			break;
	}
	return -1;
}

/*
功能:请求一级用户数据
返回:0,成功,并向采集模块上报数据;-1,失败,pslave_node节点状态重设为STATE_UNINITED
*/
int ptrcl102_req_level1_data(Slave_102_node* pslave_node)
{
	Tprtcl102_fixed_frame reql1_frame = {0};
	unsigned char resp_buf[MAX_FRAME_LENGTH] = {0};

	pslave_node->fcb = (~(pslave_node->fcb)) & 0x1;

	unsigned char ctrl = COMBINE_CTRL(1, pslave_node->fcb, 1, C_PL1_NA_2);
	unsigned short slave_id = pslave_node->slave_id;
	int fd = pslave_node->fd;

	int i = 3;
	while(i)
	{
		i--;
		pack_fix_frame(ctrl,slave_id,(unsigned char*)&reql1_frame);
		int ret = serial_send_data(fd, (unsigned char*)&reql1_frame, sizeof(Tprtcl102_fixed_frame));
		if(ret < 0)
		{
			continue;
		}

		ret = receive_frame(pslave_node, resp_buf);
		if(ret < 0)
		{
			continue;
		}

		ret = parse_respon_frame((unsigned char*)&reql1_frame,(unsigned char*)resp_buf,pslave_node);
		if(ret == 0)
		{
			break;
		}
		else if(ret == 1)	// 表示还有一级用户数据需要读取，重新设置轮询次数，并更新fcb
		{
			i = 3;
			pslave_node->fcb = (~(pslave_node->fcb)) & 0x1;
			ctrl = COMBINE_CTRL(1, pslave_node->fcb, 1, C_PL1_NA_2);
			continue;
		}
		else
		{
			continue;
		}
	}

	if(i <= 0)
	{
		return -1;
	}

	return 0;
}

/* 通信初始化 */
int comm_init(Slave_102_node* pslave_node)
{
    int i = 0;
    int fd = pslave_node->fd;
    
    unsigned char ctl_flag = COMBINE_CTRL(1, 0, 0, C_RCU_NA_2);
    Tprtcl102_fixed_frame reset_frame = {0};
    unsigned char resp_buf[6] = {0};
    unsigned short slave_id = pslave_node->slave_id;
    
    pslave_node->fcb = 0;
    
    for(i = 0; i < 3; i++)
    {
        pack_fix_frame(ctl_flag, slave_id, (unsigned char*)&reset_frame);
        int ret = serial_send_data(fd, (unsigned char*)&reset_frame, sizeof(Tprtcl102_fixed_frame));
        if(ret < 0)
        {
            continue;
        }
        
        ret = receive_frame(pslave_node, (unsigned char*)&resp_buf);
        if(ret < 0)
        {
            continue;
        }

        ret = parse_respon_frame((unsigned char*)&reset_frame, resp_buf, pslave_node);
        if(ret == 0)
        {
            return 0;
        }
        if(ret == 1)
        {
            ptrcl102_req_level1_data(pslave_node);     // 读一级用户数据
            return 0;
        }
        else
        {
            continue;
        }
    }
    
    return -1;
}

/* 召唤当前数据 */
int refer_now_data(Slave_102_node* pSla_node, unsigned char data_type)
{
	/*召唤当前数据报文*/
	unsigned char refer_data[8] = {0x00, 0x01, 0x06, 0x00, 0x00, 0x00, 0x01, 0xff};
	refer_data[0] = data_type;
    refer_data[3] = (unsigned char)pSla_node->slave_id;
    refer_data[4] = (unsigned char)(pSla_node->slave_id >> 8);

	int ret = 0;

	pSla_node->fcb = (~(pSla_node->fcb)) & 0x1;
	unsigned char ctrl = COMBINE_CTRL(1, pSla_node->fcb,1, C_DR_NA_3);

	unsigned char ttl_ref_frame[16]={0};	
	unsigned char ttl_ref_resp_frame[MAX_FRAME_LENGTH] = {0};

	int i = 0;
	for(i = 0; i < 3; i++)
    {
        pack_var_frame(ctrl, pSla_node->slave_id, refer_data, 8, (unsigned char*)&ttl_ref_frame);
        ret = serial_send_data(pSla_node->fd, (unsigned char*)&ttl_ref_frame, 16);
        if(ret < 0)
        {
            continue;
        }
        
        ret = receive_frame(pSla_node, (unsigned char*)&ttl_ref_resp_frame);
        if(ret < 0)
        {
            continue;
        }

        ret = parse_respon_frame((unsigned char*)&ttl_ref_frame, (unsigned char*)&ttl_ref_resp_frame, pSla_node);
        if(ret == 0) // 执行成功
        {
        	return 0;
        }
        else if(ret == 1) // 要求请求一级用户数据
        {
        	if(ptrcl102_req_level1_data(pSla_node) < 0)
        	{
        		continue;
        	}
        	else
        	{
        		return 0;
        	}
        }
        else
        {
        	continue;
        }
    }
    return -1;
}

/* 召唤历史数据 */
/* 参数start_time和end_time的格式为: char[5] = [min, hour, day, month, year % 100] */
int refer_history_data(Slave_102_node* pSla_node, unsigned char data_type, char* start_time, char* end_time)
{
	/*召唤当前数据报文*/
	unsigned char refer_data[18] = {0x00, 0x01, 0x06, 0x00, 0x00, 0x00, 0x01, 0xff, 
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                                    0x00, 0x00};
	refer_data[0] = data_type;
    refer_data[3] = (unsigned char)pSla_node->slave_id;
    refer_data[4] = (unsigned char)(pSla_node->slave_id >> 8);
    memcpy(&refer_data[8], start_time, 5);
    memcpy(&refer_data[13], end_time, 5);

	int ret = 0;

	pSla_node->fcb = (~(pSla_node->fcb)) & 0x1;
	unsigned char ctrl = COMBINE_CTRL(1, pSla_node->fcb,1, C_DR_NA_3);

	unsigned char ttl_ref_frame[26]={0};	
	unsigned char ttl_ref_resp_frame[MAX_FRAME_LENGTH] = {0};

	int i = 0;
	for(i = 0; i < 3; i++)
    {
        pack_var_frame(ctrl, pSla_node->slave_id, refer_data, 18, (unsigned char*)&ttl_ref_frame);
        ret = serial_send_data(pSla_node->fd, (unsigned char*)&ttl_ref_frame, 26);
        if(ret < 0)
        {
            continue;
        }
        
        ret = receive_frame(pSla_node, (unsigned char*)&ttl_ref_resp_frame);
        if(ret < 0)
        {
            continue;
        }

        ret = parse_respon_frame((unsigned char*)&ttl_ref_frame, (unsigned char*)&ttl_ref_resp_frame, pSla_node);
        if(ret == 0) // 执行成功
        {
        	return 0;
        }
        else if(ret == 1) // 要求请求一级用户数据
        {
        	if(ptrcl102_req_level1_data(pSla_node) < 0)
        	{
        		continue;
        	}
        	else
        	{
        		return 0;
        	}
        }
        else
        {
        	continue;
        }
    }
    return -1;
}

/* 解析配置文件 */
int parse102_config(char* config_str, Config_102_info* out_config)
{
    int i = 0, j = 0;
    cJSON *json_root = cJSON_Parse(config_str);
    if(json_root == NULL)
    {
        return -1;
    }

    cJSON *json_item = cJSON_GetObjectItem(json_root, "message");
    if(json_item == NULL)
    {
        return -1;
    }

    cJSON* json_table = cJSON_GetObjectItem(json_item, "protocol");
    if(0 != strcmp(json_table->valuestring, "iec102"))
    {
        return -1;
    }

    json_table = cJSON_GetObjectItem(json_item, "port"); // 获取串口port
    strcpy(out_config->port, json_table->valuestring);

    json_table = cJSON_GetObjectItem(json_item, "classify"); 
    cJSON* json_subarray = cJSON_GetArrayItem(json_table, 0);

    cJSON* json_array = cJSON_GetObjectItem(json_subarray,"device_addr");
    int device_num = cJSON_GetArraySize(json_array);
    out_config->device_num = device_num;

    cJSON *json_data, *json_data1, *json_data2, *json_data3;
    
    for(i = 0; i < device_num; i++)
    {
        json_data = cJSON_GetArrayItem(json_array, i);
        out_config->device_addr[i] = (unsigned char)(json_data->valuestring[0] - '0');
    }

    int post = 0;
    json_array = cJSON_GetObjectItem(json_subarray,"state_table");
    int table_num = cJSON_GetArraySize(json_array);

    for(i = 0; i < table_num; i++)
    {
        json_data = cJSON_GetArrayItem(json_array, i);

        json_data1 = cJSON_GetArrayItem(json_data, 0);
        out_config->catch_list[post].record_addr = json_data1->valueint;

        json_data1 = cJSON_GetArrayItem(json_data, 1);
        out_config->catch_list[post].info_addr = json_data1->valueint;

        json_data1 = cJSON_GetArrayItem(json_data, 2);
        strcpy(out_config->catch_list[post].point_name, json_data1->valuestring);
        post++;
    }

    json_array = cJSON_GetObjectItem(json_subarray,"message_table");
    int message_num = cJSON_GetArraySize(json_array);

    for(i = 0; i < message_num; i++)
    {
        json_data = cJSON_GetArrayItem(json_array, i);

        json_data1 = cJSON_GetObjectItem(json_data, "group");
        unsigned char record_id = json_data1->valueint;

        json_data1 = cJSON_GetObjectItem(json_data, "setting");
        int set_num = cJSON_GetArraySize(json_data1);

        for(j = 0; j < set_num; j++)
        {
            json_data2 = cJSON_GetArrayItem(json_data1, j);

            out_config->catch_list[post].record_addr = record_id;

            json_data3 = cJSON_GetArrayItem(json_data2, 0);
            out_config->catch_list[post].info_addr = json_data3->valueint;

            json_data3 = cJSON_GetArrayItem(json_data2, 1);
            strcpy(out_config->catch_list[post].point_name, json_data3->valuestring);
            post++;
        }
    }

    out_config->catch_num = post;
    cJSON_Delete(json_root);
    return 0;
}

void clear102_data(void)
{
    data102_post = 0; // 清空数据
    memset(prot102_data, 0, sizeof(Protocol102_data)*1000);
}

/* 生成json格式数据，并写入共享内存 */
int make_102_data(Config_102_info* conf, Protocol_data_sm* data_sm)
{
    time_t t;
    t = time(NULL);
    int now_time = time(&t);

    int i = 0, j = 0, k = 0;

    if(data102_post == 0)
    {
        return -1;
    }
    // 现在只支持一台设备，打开注释，可以支持多台设备
    // cJSON* cjson_root = cJSON_CreateObject();

    // for(k = 0; k < conf->device_num; k++)
    // {
        cJSON* cjson_array = cJSON_CreateArray();
        cJSON* cjson_item = NULL;

        for(i = 0; i < conf->catch_num; i++)
        {
            cjson_item = cJSON_CreateObject();
            cJSON_AddStringToObject(cjson_item, "id", conf->catch_list[i].point_name);

            for(j = 0; j < data102_post; j++)
            {
                if(conf->device_addr[k] == prot102_data[j].slave_addr &&
                    conf->catch_list[i].record_addr == prot102_data[j].record_id &&
                    conf->catch_list[i].info_addr == prot102_data[j].info_id)
                {
                    cJSON_AddNumberToObject(cjson_item, "value", prot102_data[j].data);
                    break;
                }
            }

            if(j >= data102_post)
            {
                cJSON_AddNumberToObject(cjson_item, "value", 0);
            }

            cJSON_AddNumberToObject(cjson_item, "time", now_time);

            cJSON_AddItemToArray(cjson_array, cjson_item);
        }

    //     char slave[5] = {0};
    //     sprintf(slave, "%d", conf->device_addr[k]);
    //     cJSON_AddItemToObject(cjson_root, slave, cjson_array);
    // }

    /* 打印JSON对象(整条链表)的所有数据 */
    // char* str = cJSON_Print(cjson_root);  // 支持多台设备时使用这句

    char* str = cJSON_Print(cjson_array);  // 支持多台设备时注释掉这句

    pthread_rwlock_wrlock(&data_sm->rwlock);
    memset(data_sm->protocol_data, 0, PROTOCOL103_DATA_LEN);  // 清空数据区
    strcpy(data_sm->protocol_data, str);    // 将结果写入数据区
    pthread_rwlock_unlock(&data_sm->rwlock);

    // cJSON_Delete(cjson_root);  // 支持多台设备时使用这句
    cJSON_Delete(cjson_array);  // 支持多台设备时注释掉这句
    return 0;
}

void showconfig(Config_102_info* conf)
{
    int i = 0;
    printf("port: %s\n", conf->port);
    printf("device_num: %d\n", conf->device_num);
    printf("addr: ");
    for(i = 0; i < conf->device_num; i++)
    {
        printf("%d ", conf->device_addr[i]);
    }
    printf("\n");

    printf("catch_num: %d\n", conf->catch_num);
    printf("conf: ");
    for(i = 0; i < conf->catch_num; i++)
    {
        printf("[%d]: %d %d %s\n", i, conf->catch_list[i].record_addr, 
            conf->catch_list[i].info_addr, conf->catch_list[i].point_name);
    }
}

void protocol102_main(void)
{
    /* 挂接配置文件的共享内存 */
    Protocol_config_sm* pconfig102_sm = get_shared_memory(PROTOCOL102_CONFIG_SM_KEY);
    /* 挂接数据文件的共享内存 */
    Protocol_data_sm* pdata102_sm = get_shared_memory(PROTOCOL102_DATA_SM_KEY);

    Config_102_info config102_info = {0};   // 解析json后的配置文件

    while(1)
    {
        int ret = 0;
        /* 配置文件需要每次都解析，这样配置文件更新时可以及时根据新的配置文件来获取数据 */
        pthread_rwlock_wrlock(&pconfig102_sm->rwlock);
        if(pconfig102_sm->started != 1)   // 判断启动标志位，如果不是为启动，直接退出
        {
            pthread_rwlock_unlock(&pconfig102_sm->rwlock);
            sleep(10);
            continue;
        }
        ret = parse102_config(pconfig102_sm->config_data, &config102_info);
        if(ret == -1)
        {
            pthread_rwlock_unlock(&pconfig102_sm->rwlock);
            sleep(10);
            continue;
        }
        pthread_rwlock_unlock(&pconfig102_sm->rwlock);

        showconfig(&config102_info);
        int serial_fd = open(config102_info.port, O_RDWR);
        if(serial_fd < 0)
        {
            sleep(1);
            continue;
        }

       
        ret = set_serial(serial_fd, 9600, 8, 1, 'n');
        if(ret < 0)
        {
            sleep(1);
            continue;
        }

        clear102_data(); // 清空数据缓冲区

        /* 获取数据 */
        int i = 0;
        for(i = 0; i < config102_info.device_num; i++)
        {
            Slave_102_node slave_node = {0};
            slave_node.fd = serial_fd;
            slave_node.slave_id = config102_info.device_addr[i];

            ret = comm_init(&slave_node);   // 初始化
            if(ret < 0)
            {
                continue;
            }

            ret = refer_now_data(&slave_node, C_RD_DLA_2);   // 召唤当前电量数据
            if(ret < 0)
            {
                continue;
            }

            ret = refer_now_data(&slave_node, C_SP_NA_2);   // 召唤当前单点信息
            if(ret < 0)
            {
                continue;
            }
        }

        /* 生成josn结果，并写入共享内存 */
        make_102_data(&config102_info, pdata102_sm);

        close(serial_fd);

        sleep(10);
    }
}
