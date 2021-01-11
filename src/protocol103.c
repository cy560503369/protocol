#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "protocol.h"
#include "protocol103.h"
#include "uart.h"

/* 103数据缓存区，先弄成固定长度的，后面要求高的话，可以改成链表 */
Protocol103_data prot103_data[1000] = {0};
int data_post = 0;   // 当前数据偏移

/* 获得微秒时间 */
unsigned long get_ms_time()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000); //ms
    //debug("ts.tv_sec %u ts.tv_nsec %u\n",ts.tv_sec,ts.tv_nsec);
    //return (ts.tv_sec * 1000000 + ts.tv_nsec / 1000); //us
}
/**********************************************************************
*	函数名称：unsigned char cal_cs(unsigned char *cal_data, unsigned short data_len)	
*	函数功能：计算数据的校验和
**********************************************************************/
unsigned char cal_cs(unsigned char *cal_data, unsigned short data_len)
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
int pack_fixed_frame(unsigned char contrl, unsigned char slave_addr, 
                     unsigned char* frame)
{
	if(frame == NULL)
	{
		return -1;
	}

	frame[0] = 0x10;
	frame[1] = contrl;
	frame[2] = slave_addr;
	frame[3] = cal_cs(&frame[1], 2);
	frame[4] = 0x16;
	return 0;
}

/*
	组不定长报文
*/
int pack_variable_frame(unsigned char contrl, unsigned char slave_addr, 
                        unsigned char* data, unsigned char data_len, 
                        unsigned char* frame)
{
	if(frame == NULL || data == NULL)
	{
		return -1;
	}

	frame[0] = 0x68;
	frame[1] = data_len + 2;
	frame[2] = data_len + 2;
	frame[3] = 0x68;
	frame[4] = contrl;
	frame[5] = slave_addr;
	memcpy(&frame[6], data, data_len);
	frame[6 + data_len] = cal_cs(&frame[4], data_len + 2);
	frame[7 + data_len] = 0x16;
	return 0;
}


int recv_frame(Slave_node* pslave_node, unsigned char* pframe_buf)
{
    int ret;
    int fd = pslave_node->fd;
    unsigned char* pbuf = pframe_buf;
    unsigned long utime_start = get_ms_time();

    while(1)
    {
        ret = serial_receive_data(fd, pbuf, 1);
        if((ret == 1) && (*pbuf == FIXED_FRAME_START_CHAR))  // 固定长度帧
        {
            pbuf++;
            ret = serial_receive_data(fd, pbuf, sizeof(Tprtcl103_fixed_frame)-1);
            if(ret < sizeof(Tprtcl103_fixed_frame)-1)
            {
                tcflush(fd, TCIFLUSH);
                return -1;
            }
            return 0;
        }
        else if(ret == 1 && *pbuf == UNFIXED_FRAME_START_CHAR) // 不定长度帧
        {
            pbuf++;
            // 读帧头
            ret = serial_receive_data(fd, pbuf, sizeof(Tprtcl103_unfixed_frame_head)-1);
            if(ret < sizeof(Tprtcl103_unfixed_frame_head)-1)
            {
                tcflush(fd, TCIFLUSH);
                return -1;
            }
            pbuf += sizeof(Tprtcl103_unfixed_frame_head)-1;
            // 读帧头后续内容
            int len = ((Tprtcl103_unfixed_frame_head*)pframe_buf)->len1;
            ret = serial_receive_data(fd, pbuf, len);
            if(ret < len)
            {
                tcflush(fd, TCIFLUSH);
                return -1;
            }
            return 0;
        }
        else if(ret == 1)   // 非起始字符
        {
        	if((get_ms_time() - utime_start) > 10000)    //读10s还没读到起始字符
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
        	if((get_ms_time() - utime_start) > 3000)  //读3秒没读到数据
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

int unpack_ASDU_frame(char* buff, unsigned char slave_addr)
{
	unsigned char asdu_type = buff[0];
	switch(asdu_type)
	{
		case ASDU01:
		case ASDU02:
		{
			prot103_data[data_post].slave_addr = slave_addr;
			prot103_data[data_post].type = EVENT_DATA;
			prot103_data[data_post].data.event_data.Fun = buff[4];
			prot103_data[data_post].data.event_data.Inf = buff[5];
			if((buff[6] & 0x03) == 0x02)
			{
				prot103_data[data_post].data.event_data.value = 1; //'on'
			}
			else
			{
				prot103_data[data_post].data.event_data.value = 0;  //'off'
			}
			data_post++; // 更新数据位置
		}
			break;
		case ASDU10:
		{
			unsigned char data_num = buff[7] & 0x3f;
			int i = 0, off_set = 0;
			for(i = 0; i < data_num; i++)
			{
				prot103_data[data_post].slave_addr = slave_addr;
				prot103_data[data_post].type = GROUP_DATA;
				prot103_data[data_post].data.group_data.group = buff[8 + off_set];
				prot103_data[data_post].data.group_data.entry = buff[9 + off_set];
				
				memcpy(&prot103_data[data_post].data.group_data.value, &buff[14 + off_set], 4);

				off_set += 10;
				data_post++;
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
int parse_resp_frame(unsigned char* cmd_frame, unsigned char* resp_frame, Slave_node* pslave_node)
{
    int resp_frame_ctrl;
    int resp_frame_acd;
    int resp_frame_fun;
    unsigned char cmd_slave_addr,resp_slave_addr;
     
    Tprtcl103_fixed_frame* pfixed_cmd_frame = NULL;
    Tprtcl103_fixed_frame* pfixed_resp_frame = NULL;
    Tprtcl103_unfixed_frame_head* punfixed_cmd_frame = NULL;
    Tprtcl103_unfixed_frame_head* punfixed_resp_frame = NULL;
     
    if((NULL == cmd_frame) || (NULL == resp_frame)
        || ((*cmd_frame != FIXED_FRAME_START_CHAR) && (*cmd_frame != UNFIXED_FRAME_START_CHAR))
        || ((*resp_frame != FIXED_FRAME_START_CHAR) && (*resp_frame != UNFIXED_FRAME_START_CHAR)))
    {
        return -1;
    }
    
    if(*cmd_frame == FIXED_FRAME_START_CHAR)
    {
        pfixed_cmd_frame = (Tprtcl103_fixed_frame*)cmd_frame;
        cmd_slave_addr = pfixed_cmd_frame->slave_addr;
    }
    else
    {

        punfixed_cmd_frame = (Tprtcl103_unfixed_frame_head*)cmd_frame;
        cmd_slave_addr = punfixed_cmd_frame->slave_addr;
    }
    
    if(*resp_frame == FIXED_FRAME_START_CHAR)
    {
        pfixed_resp_frame = (Tprtcl103_fixed_frame*)resp_frame;
        /* 校验crc */
        if(cal_cs(&pfixed_resp_frame->ctrl, 2) != pfixed_resp_frame->crc)
        {
        	return -1;
        }
        resp_frame_ctrl = pfixed_resp_frame->ctrl;
		resp_slave_addr = pfixed_resp_frame->slave_addr;  
    }
    else
    {
        punfixed_resp_frame = (Tprtcl103_unfixed_frame_head*)resp_frame;
        /* 校验crc */
        if(cal_cs(&punfixed_resp_frame->ctrl, punfixed_resp_frame->len1) != 
        	*(resp_frame + punfixed_resp_frame->len1 + 4))
        {
        	return -1;
        }
        resp_frame_ctrl = punfixed_resp_frame->ctrl;
		resp_slave_addr = punfixed_resp_frame->slave_addr;
    }

    resp_frame_acd = FETCH_ACD(resp_frame_ctrl);
	resp_frame_fun = FETCH_FUN(resp_frame_ctrl);

	if(cmd_slave_addr != resp_slave_addr)
	{
		return -1;
	}

	switch(resp_frame_fun)
	{
		case M_CON_NA_3:
		case M_BY_NA_3:	
		case M_NV_NA_3:
		case M_LKR_NA_3:
			return resp_frame_acd;
		case M_DR_NA_3:
			unpack_ASDU_frame((char*)(resp_frame+6),pslave_node->slave_id);
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
int ptrcl103_req_level1_data(Slave_node* pslave_node)
{
	Tprtcl103_fixed_frame reql1_frame = {0};
	unsigned char resp_buf[MAX_FRAME_LEN] = {0};

	pslave_node->fcb = (~(pslave_node->fcb)) & 0x1;

	unsigned char ctrl = COMB_CTRL(1, pslave_node->fcb, 1, C_PL1_NA_3);
	unsigned char slave_id = pslave_node->slave_id;
	int fd = pslave_node->fd;

	int i = 3;
	while(i)
	{
		i--;
		pack_fixed_frame(ctrl,slave_id,(unsigned char*)&reql1_frame);
		int ret = serial_send_data(fd, (unsigned char*)&reql1_frame, sizeof(Tprtcl103_fixed_frame));
		if(ret < 0)
		{
			continue;
		}

		ret = recv_frame(pslave_node, resp_buf);
		if(ret < 0)
		{
			continue;
		}

		ret = parse_resp_frame((unsigned char*)&reql1_frame,(unsigned char*)resp_buf,pslave_node);
		if(ret == 0)
		{
			break;
		}
		else if(ret == 1)	// 表示还有一级用户数据需要读取，重新设置轮询次数，并更新fcb
		{
			i = 3;
			pslave_node->fcb = (~(pslave_node->fcb)) & 0x1;
			ctrl = COMB_CTRL(1, pslave_node->fcb, 1, C_PL1_NA_3);
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
int communicate_init(Slave_node* pslave_node)
{
    int i = 0;
    int fd = pslave_node->fd;
    
    unsigned char ctl_flag = COMB_CTRL(1, 0, 0, C_RCU_NA_3);
    Tprtcl103_fixed_frame reset_frame = {0};
    Tprtcl103_fixed_frame reset_resp_frame = {0};
    unsigned char slave_id = pslave_node->slave_id;
    
    pslave_node->fcb = 0;
    
    for(i = 0; i < 3; i++)
    {
        pack_fixed_frame(ctl_flag, slave_id, (unsigned char*)&reset_frame);
        int ret = serial_send_data(fd, (unsigned char*)&reset_frame, sizeof(Tprtcl103_fixed_frame));
        if(ret < 0)
        {
            continue;
        }
        
        ret = recv_frame(pslave_node, (unsigned char*)&reset_resp_frame);
        if(ret < 0)
        {
            continue;
        }

        ret = parse_resp_frame((unsigned char*)&reset_frame, (unsigned char*)&reset_resp_frame, pslave_node);
        if(ret == 0) // 执行成功
        {
        	return 0;
        }
        else if(ret == 1) // 要求请求一级用户数据
        {
        	ptrcl103_req_level1_data(pslave_node);
        	return 0;
        }
        else
        {
        	continue;
        }
    }
    
    return -1;
}

/* 总召唤 */
int total_refer(Slave_node* pSla_node)
{
	/*总召唤报文*/
	unsigned char total_refer_data[7] = {0x07, 0x81, 0x09, 0x00, 0xff, 0x00, 0x00};
	total_refer_data[3] = pSla_node->slave_id;

	int ret = 0;

	pSla_node->fcb = (~(pSla_node->fcb)) & 0x1;
	unsigned char ctrl = COMB_CTRL(1, pSla_node->fcb,1, C_DR_NA_3);

	unsigned char ttl_ref_frame[15]={0};	
	unsigned char ttl_ref_resp_frame[MAX_FRAME_LEN] = {0};

	int i = 0;
	for(i = 0; i < 3; i++)
    {
        pack_variable_frame(ctrl, pSla_node->slave_id, total_refer_data, 7, (unsigned char*)&ttl_ref_frame);
        ret = serial_send_data(pSla_node->fd, (unsigned char*)&ttl_ref_frame, 15);
        if(ret < 0)
        {
            continue;
        }
        
        ret = recv_frame(pSla_node, (unsigned char*)&ttl_ref_resp_frame);
        if(ret < 0)
        {
            continue;
        }

        ret = parse_resp_frame((unsigned char*)&ttl_ref_frame, (unsigned char*)&ttl_ref_resp_frame, pSla_node);
        if(ret == 0) // 执行成功
        {
        	return 0;
        }
        else if(ret == 1) // 要求请求一级用户数据
        {
        	if(ptrcl103_req_level1_data(pSla_node) < 0)
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

/* 读组值 */
int get_group_id(Slave_node* pSla_node, unsigned char group_num)
{
    /*总召唤报文*/
	unsigned char group_data[11] = {0x15, 0x81, 0x2a, 0x00, 0xfe, 0xf1, 0x00, 0x01,0x00, 0x00, 0x01};
	group_data[3] = pSla_node->slave_id;
    group_data[8] = group_num;

	int ret = 0;

	pSla_node->fcb = (~(pSla_node->fcb)) & 0x1;
	unsigned char ctrl = COMB_CTRL(1, pSla_node->fcb,1, C_DR_NA_3);

	unsigned char group_frame[19]={0};	
	unsigned char group_resp_frame[MAX_FRAME_LEN] = {0};

	int i = 0;
	for(i = 0; i < 3; i++)
    {
        pack_variable_frame(ctrl, pSla_node->slave_id, group_data, 11, (unsigned char*)&group_frame);

        ret = serial_send_data(pSla_node->fd, (unsigned char*)&group_frame, 19);
        if(ret < 0)
        {
            continue;
        }
        
        ret = recv_frame(pSla_node, (unsigned char*)&group_resp_frame);
        if(ret < 0)
        {
            continue;
        }

        ret = parse_resp_frame((unsigned char*)&group_frame, (unsigned char*)&group_resp_frame, pSla_node);
        if(ret == 0) // 执行成功
        {
        	return 0;
        }
        else if(ret == 1) // 要求请求一级用户数据
        {
        	if(ptrcl103_req_level1_data(pSla_node) < 0)
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
int parse_config(char* config_str, Config_info* out_config)
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
    if(0 != strcmp(json_table->valuestring, "iec103"))
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

    json_array = cJSON_GetObjectItem(json_subarray,"state_table");
    int table_num = cJSON_GetArraySize(json_array);
    out_config->state_table_num = table_num;

    for(i = 0; i < table_num; i++)
    {
        json_data = cJSON_GetArrayItem(json_array, i);

        json_data1 = cJSON_GetArrayItem(json_data, 0);
        out_config->state_table[i].Fun = json_data1->valueint;

        json_data1 = cJSON_GetArrayItem(json_data, 1);
        out_config->state_table[i].Inf = json_data1->valueint;

        json_data1 = cJSON_GetArrayItem(json_data, 2);
        strcpy(out_config->state_table[i].id_name, json_data1->valuestring);
    }

    json_array = cJSON_GetObjectItem(json_subarray,"message_table");
    int message_num = cJSON_GetArraySize(json_array);
    out_config->message_table_num = message_num;

    for(i = 0; i < message_num; i++)
    {
        json_data = cJSON_GetArrayItem(json_array, i);

        json_data1 = cJSON_GetObjectItem(json_data, "group");
        out_config->message_table[i].group = json_data1->valueint;

        json_data1 = cJSON_GetObjectItem(json_data, "setting");
        int set_num = cJSON_GetArraySize(json_data1);
        out_config->message_table[i].entry_num = set_num;

        for(j = 0; j < set_num; j++)
        {
            json_data2 = cJSON_GetArrayItem(json_data1, j);

            json_data3 = cJSON_GetArrayItem(json_data2, 0);
            out_config->message_table[i].entry_info[j].entry = json_data3->valueint;

            json_data3 = cJSON_GetArrayItem(json_data2, 1);
            strcpy(out_config->message_table[i].entry_info[j].id_name, json_data3->valuestring);
        }
    }

    cJSON_Delete(json_root);
    return 0;
}

void clear_data(void)
{
    data_post = 0; // 清空数据
    memset(prot103_data, 0, sizeof(Protocol103_data)*1000);
}

/* 生成json格式数据，并写入共享内存 */
int make_json_data(Config_info* conf, Protocol_data_sm* data_sm)
{
    time_t t;
    t = time(NULL);
    int now_time = time(&t);

    int i = 0, j = 0, k = 0, l = 0;

    // 现在只支持一台设备，打开注释，可以支持多台设备
    // cJSON* cjson_root = cJSON_CreateObject();

    // for(k = 0; k < conf->device_num; k++)
    // {
        cJSON* cjson_array = cJSON_CreateArray();
        cJSON* cjson_item = NULL;

        for(i = 0; i < conf->state_table_num; i++)
        {
            cjson_item = cJSON_CreateObject();
            cJSON_AddStringToObject(cjson_item, "id", conf->state_table[i].id_name);

            for(j = 0; j < data_post; j++)
            {
                if((conf->device_addr[k] == prot103_data[j].slave_addr) && 
                    (prot103_data[j].type == EVENT_DATA))
                {
                    if((conf->state_table[i].Fun == prot103_data[j].data.event_data.Fun) &&
                        (conf->state_table[i].Inf == prot103_data[j].data.event_data.Inf))
                    {
                        cJSON_AddNumberToObject(cjson_item, "value", prot103_data[j].data.event_data.value);
                        break;
                    }
                }
                else
                {
                    continue;
                }
            }

            if(j >= data_post)
            {
                cJSON_AddNumberToObject(cjson_item, "value", 0);
            }

            cJSON_AddNumberToObject(cjson_item, "time", now_time);

            cJSON_AddItemToArray(cjson_array, cjson_item);
        }

        for(i = 0; i < conf->message_table_num; i++)
        {
            for(l = 0; l < conf->message_table[i].entry_num; l++)
            {
                cjson_item = cJSON_CreateObject();
                cJSON_AddStringToObject(cjson_item, "id", conf->message_table[i].entry_info[l].id_name);

                for(j = 0; j < data_post; j++)
                {
                    if((conf->device_addr[k] == prot103_data[j].slave_addr) && 
                        (prot103_data[j].type == GROUP_DATA))
                    {
                        if((conf->message_table[i].group == prot103_data[j].data.group_data.group) &&
                            (conf->message_table[i].entry_info[l].entry == prot103_data[j].data.group_data.entry))
                        {
                            cJSON_AddNumberToObject(cjson_item, "value", prot103_data[j].data.group_data.value);
                            break;
                        }
                    }
                    else
                    {
                        continue;
                    }
                }

                if(j >= data_post)
                {
                    cJSON_AddNumberToObject(cjson_item, "value", 0);
                }
                cJSON_AddNumberToObject(cjson_item, "time", now_time);
                cJSON_AddItemToArray(cjson_array, cjson_item);
            }
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

void protocol103_main(void)
{
    /* 挂接配置文件的共享内存 */
    Protocol_config_sm* pconfig_sm = get_shared_memory(PROTOCOL103_CONFIG_SM_KEY);
    /* 挂接数据文件的共享内存 */
    Protocol_data_sm* pdata_sm = get_shared_memory(PROTOCOL103_DATA_SM_KEY);

    Config_info config_info = {0};   // 解析json后的配置文件

    while(1)
    {
        /* 配置文件需要每次都解析，这样配置文件更新时可以及时根据新的配置文件来获取数据 */
        pthread_rwlock_wrlock(&pconfig_sm->rwlock);
        if(pconfig_sm->started != 1)   // 判断启动标志位，如果不是为启动，直接退出
        {
            pthread_rwlock_unlock(&pconfig_sm->rwlock);
            sleep(10);
            continue;
        }
        parse_config(pconfig_sm->config_data, &config_info);
        pthread_rwlock_unlock(&pconfig_sm->rwlock);

        int serial_fd = open(config_info.port, O_RDWR);
        if(serial_fd < 0)
        {
            sleep(1);
            continue;
        }

        int ret = 0;
        ret = set_serial(serial_fd, 9600, 8, 1, 'n');
        if(ret < 0)
        {
            sleep(1);
            continue;
        }

        clear_data(); // 清空数据缓冲区

        /* 获取数据 */
        int i = 0, j = 0;
        for(i = 0; i < config_info.device_num; i++)
        {
            Slave_node slave_node = {0};
            slave_node.fd = serial_fd;
            slave_node.slave_id = config_info.device_addr[i];

            ret = communicate_init(&slave_node);
            if(ret < 0)
            {
                continue;
            }

            ret = total_refer(&slave_node);
            if(ret < 0)
            {
                continue;
            }

            for(j = 0; j < config_info.message_table_num; j++)
            {
                ret = get_group_id(&slave_node, config_info.message_table[j].group);
                if(ret < 0)
                {
                    break;
                }
            }
        }

        /* 生成josn结果，并写入共享内存 */
        make_json_data(&config_info, pdata_sm);

        close(serial_fd);

        sleep(60);
    }
}
