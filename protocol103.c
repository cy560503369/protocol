#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "protocol103.h"
#include "uart.h"

void show(unsigned char* data, int len)
{
    int i = 0;
    for(i = 0; i < len; i++)
    {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

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

/* 
    判断应答帧resp_frame是否为命令帧cmd_frame的正确应答
    返回值：-1，不是正确应答；0，是正确应答且acd为0；1，是正确应答且acd为1
 */
int parse_resp_frame(unsigned char* cmd_frame, unsigned char* resp_frame, Slave_node* pslave_node)
{
    int cmd_frame_type, resp_frame_type;
    int cmd_frame_ctrl, resp_frame_ctrl;
    int cmd_frame_fcb, resp_frame_acd;
    int cmd_frame_fun, resp_frame_fun;
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
        cmd_frame_type = FIXED_FRAME_TYPE;
        pfixed_cmd_frame = (Tprtcl103_fixed_frame*)cmd_frame;
        cmd_frame_ctrl = pfixed_cmd_frame->ctrl;
        cmd_slave_addr = pfixed_cmd_frame->slave_addr;
    }
    else
    {
        cmd_frame_type = UNFIXED_FRAME_TYPE;
        punfixed_cmd_frame = (Tprtcl103_unfixed_frame_head*)cmd_frame;
        cmd_frame_ctrl = punfixed_cmd_frame->ctrl;
        cmd_slave_addr = punfixed_cmd_frame->slave_addr;
    }
    
    cmd_frame_fcb = FETCH_FCB(cmd_frame_ctrl);
    cmd_frame_fun = FETCH_FUN(cmd_frame_ctrl);
    
    if(*resp_frame == FIXED_FRAME_START_CHAR)
    {
        resp_frame_type = FIXED_FRAME_TYPE;
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
        resp_frame_type = UNFIXED_FRAME_TYPE;
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
			// unpack_ASDU_frame((char*)(resp_frame+6),pslave_node->slave_id);
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
        printf("level1_data: %d\n", i);
        printf("send: ");
        show((unsigned char*)&reql1_frame, sizeof(Tprtcl103_fixed_frame));
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

        printf("recive: ");
        show(resp_buf, 30);
		ret = parse_resp_frame((unsigned char*)&reql1_frame,(unsigned char*)resp_buf,pslave_node);
        printf("level ret: %d\n", ret);
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
    int reset_flag = -1;
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
        printf("communite: %d\n", i);
        printf("send: ");
        show((unsigned char*)&reset_frame, sizeof(Tprtcl103_fixed_frame));
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
        printf("receive: ");
        show((unsigned char*)&reset_resp_frame, sizeof(Tprtcl103_fixed_frame));
        ret = parse_resp_frame((unsigned char*)&reset_frame, (unsigned char*)&reset_resp_frame, pslave_node);
        printf("commucate ret: %d\n", ret);
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
        printf("total_refer: %d\n", i);
        printf("send: ");
        show((unsigned char*)&ttl_ref_frame, 15);
        int ret = serial_send_data(pSla_node->fd, (unsigned char*)&ttl_ref_frame, 15);
        if(ret < 0)
        {
            continue;
        }
        
        ret = recv_frame(pSla_node, (unsigned char*)&ttl_ref_resp_frame);
        if(ret < 0)
        {
            continue;
        }
        printf("receive: ");
        show((unsigned char*)&ttl_ref_resp_frame, 30);
        ret = parse_resp_frame((unsigned char*)&ttl_ref_frame, (unsigned char*)&ttl_ref_resp_frame, pSla_node);
        printf("total_refer ret: %d\n", ret);
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
        printf("group: %d\n", i);
        printf("send: ");
        show((unsigned char*)&group_frame, 19);
        int ret = serial_send_data(pSla_node->fd, (unsigned char*)&group_frame, 19);
        if(ret < 0)
        {
            continue;
        }
        
        ret = recv_frame(pSla_node, (unsigned char*)&group_resp_frame);
        if(ret < 0)
        {
            continue;
        }
        printf("receive: ");
        show((unsigned char*)&group_resp_frame, 30);
        ret = parse_resp_frame((unsigned char*)&group_frame, (unsigned char*)&group_resp_frame, pSla_node);
        printf("group ret: %d\n", ret);
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

int protocol103_main(void)
{
	int serial_fd = open("/dev/ttyS1", O_RDWR);
	if(serial_fd < 0)
	{
		return -1;
	}

    int ret = 0;
    ret = set_serial(serial_fd, 9600, 8, 1, 'n');
    if(ret < 0)
    {
    	return -1;
    }

    Slave_node slave_node = {0};
    slave_node.fd = serial_fd;
    slave_node.slave_id = 1;  // 先默认为0，后面跟据配置文件进行设置

    ret = communicate_init(&slave_node);
    if(ret < 0)
    {
        close(serial_fd);
    	return -1;
    }

    ret = total_refer(&slave_node);
    if(ret < 0)
    {
        close(serial_fd);
    	return -1;
    }

    ret = get_group_id(&slave_node, 9);
    if(ret < 0)
    {
        close(serial_fd);
    	return -1;
    }

    close(serial_fd);
    return 0;
}
