/*************************************************************************
	> File Name: uart.h
	> Author:
	> Mail:
	> Created Time: 2018年06月20日 星期三 16时49分55秒
 ************************************************************************/
#ifndef MICROGRID_UART_H
#define MICROGRID_UART_H

int set_serial(int fd, int baud_rate, int databits, int stopbits, int parity);
int serial_send_data(int fd, unsigned char* data_buf, unsigned int len);
int serial_receive_data(int fd, unsigned char* data_buf, unsigned int len);

#endif //MICROGRID_UART_H
