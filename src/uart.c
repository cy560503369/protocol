/*************************************************************************
	> File Name: uart.c
	> Author:
	> Mail:
	> Created Time: 2018年06月20日 星期三 16时49分55秒
 ************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "uart.h"

unsigned long get_now_ms_times()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000); //ms
}

int set_serial(int fd, int baud_rate, int databits, int stopbits, int parity)
{
    struct termios options;
    int speed;

    memset(&options, 0, sizeof(struct termios));
    if(tcgetattr(fd, &options)  !=  0)
    {
        perror("SetupSerial 1");
        return -1;
    }

    /*选择波特率*/
    switch (baud_rate)
    {
        case 2400:
        {
            speed = B2400;
        }
            break;
        case 4800:
        {
            speed = B4800;
        }
            break;
        case 9600:
        {
            speed = B9600;
        }
            break;
        case 19200:
        {
            speed = B19200;
        }
            break;
        case 38400:
        {
            speed = B38400;
        }
            break;

        default:
        case 115200:
        {
            speed = B115200;
        }
            break;
    }
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    options.c_cflag |= (CLOCAL | CREAD);
    /* Mask the character size bits */
    options.c_cflag &= ~CSIZE;
    /*原始输入，输入字符只是被原封不动的接收*/
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    /*原始输出方式*/
    options.c_oflag &= ~(ONLCR | OCRNL | IGNCR);
    options.c_iflag &= ~(INLCR | ICRNL | IGNCR);
    /*关闭软件控制流*/
    options.c_iflag &= ~(IXON | IXOFF | IXANY);

    switch (databits) /*设置数据位数*/
    {
        case 7:
            options.c_cflag |= CS7;
            break;

        case 8:
            options.c_cflag |= CS8;
            break;

        default:
            fprintf(stderr,"Unsupported data size\n");
            return -1;
    }

    switch (parity)
    {
        case 'n':
        case 'N':
            options.c_cflag &= ~PARENB;    /* Clear parity enable */
            options.c_cflag &= ~CSTOPB;
            break;
        case 'o':
        case 'O':
            options.c_cflag |= (PARODD | PARENB); /* 设置为奇效验*/
            options.c_iflag |= INPCK;             /* Disnable parity checking */
            break;
        case 'e':
        case 'E':
            options.c_cflag |= PARENB;     /* Enable parity */
            options.c_cflag &= ~PARODD;   /* 转换为偶效验*/
            options.c_iflag |= INPCK;       /* Disnable parity checking */
            break;
        case 'S':
        case 's':  /*as no parity*/
            options.c_cflag &= ~PARENB;
            options.c_cflag &= ~CSTOPB;
            break;
        default:
            fprintf(stderr,"Unsupported parity\n");
            return -1;
    }
    /* 设置停止位*/

    switch (stopbits)
    {
        case 1:
            options.c_cflag &= ~CSTOPB;
            break;
        case 2:
            options.c_cflag |= CSTOPB;
            break;
        default:
            fprintf(stderr,"Unsupported stop bits\n");
            return -1;
    }

    /* Set input parity option */
    if (parity != 'n')
        options.c_iflag |= INPCK;

    tcflush(fd,TCIFLUSH);
    options.c_cc[VTIME] = 20; /* 设置超时15 seconds*/
    options.c_cc[VMIN] = 0; /* Update the options and do it NOW */
    if (tcsetattr(fd,TCSANOW,&options) != 0)
    {
        perror("SetupSerial 3");
        return -1;
    }
    return 0;
}

int serial_send_data(int fd, unsigned char* data_buf, unsigned int len)
{
    size_t	nleft;
	ssize_t nwritten;
    long timestart = get_now_ms_times();
	long lefttime = 10000;

    nleft = len;

    while(nleft > 0)
    {
        if(lefttime < 0)
        {
            break;
        }

        if((nwritten = write(fd, data_buf, nleft)) <= 0)
        {
            if((nwritten < 0) && (errno == EINTR))
            {
                nwritten = 0;
            }
            else
            {
                return -1;
            }
        }
        nleft -= nwritten;
        data_buf += nwritten;

        lefttime = 10000 - (get_now_ms_times() - timestart);
    }
    
    return (len - nleft);
}

int tread(int fd, unsigned char *buf, size_t nbytes, unsigned long ustimeout)
{
	int nfds;
	fd_set	readfds;
	struct timeval	tv;
	
	tv.tv_sec = 0;
	tv.tv_usec = ustimeout;
	
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	
	nfds = select(fd+1, &readfds, NULL, NULL, &tv);
	
	if (nfds <= 0) 
	{
		  if (nfds == 0)
			  errno = ETIME;
		  return(-1);
	}
	return(read(fd, buf, nbytes));
}

int serial_receive_data(int fd, unsigned char* data_buf, unsigned int len)
{
	unsigned int nleft = len;
	unsigned char *p = data_buf;
	ssize_t nread = 0;
	unsigned long lefttime = 10000; // 最长读取时间10s钟，如果字符太多，10s接收不完的话可将该值增加

	while(nleft)
	{
		unsigned long now_time = get_now_ms_times();
		if(lefttime < 0)
		{
			break;
		}
		if((nread = tread(fd, p, nleft, lefttime)) < 0)
        {
            if(errno == EINTR)
            {
                nread = 0;
            }
            else if (nleft == len)
            {
				return(-1); /* error, return -1 */
            }
			else
			{
				break;	
			}
        }
        else if(nread == 0)
        {
            break;
        }

		lefttime = lefttime - (get_now_ms_times() - now_time);

		nleft -= nread;
		p += nread;
	}
    return (len - nleft);
}
