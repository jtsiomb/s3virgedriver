/*
S3 Virge driver hack
Copyright (C) 2021 John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <conio.h>
#include <unistd.h>
#include <fcntl.h>
#include "logger.h"

static int setup_serial(void);
static void ser_putchar(int c);
static void ser_puts(const char *s);

static int sdev = -1;

int init_logger(const char *fname)
{
	int fd;

	if(sscanf(fname, "COM%d", &sdev) == 1 || sscanf(fname, "com%d", &sdev) == 1) {
		sdev--;
		if(setup_serial() == -1) {
			sdev = -1;
			fprintf(stderr, "init_logger: failed to setup serial port (%s)\n", fname);
			return -1;
		}
		return 0;
	}

	if((fd = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1) {
		fprintf(stderr, "init_logger: failed to open %s: %s\n", fname, strerror(errno));
		return -1;
	}

	close(1);
	close(2);
	dup(fd);
	dup(fd);
	return 0;
}

void stop_logger(void)
{
	sdev = -1;
	fflush(stdout);
	fflush(stderr);
	init_logger("CON");
}

void logmsg(const char *fmt, ...)
{
	static char buf[512];
	va_list ap;

	va_start(ap, fmt);
	if(sdev != -1) {
		vsprintf(buf, fmt, ap);
		ser_puts(buf);
	} else {
		vprintf(fmt, ap);
	}
	va_end(ap);
}

#define UART1_BASE		0x3f8
#define UART2_BASE		0x2f8

#define UART_DATA		0
#define UART_DIVLO		0
#define UART_DIVHI		1
#define UART_FIFO		2
#define UART_LCTL		3
#define UART_MCTL		4
#define UART_LSTAT		5

#define DIV_9600			(115200 / 9600)
#define DIV_38400			(115200 / 38400)
#define LCTL_8N1			0x03
#define LCTL_DLAB			0x80
#define FIFO_ENABLE_CLEAR	0x07
#define MCTL_DTR_RTS_OUT2	0x0b
#define LST_TREG_EMPTY		0x20

static unsigned int iobase;

static int setup_serial(void)
{
	switch(sdev) {
	case 0:
		iobase = UART1_BASE;
		break;
	case 1:
		iobase = UART2_BASE;
		break;
	default:
		sdev = -1;
		return -1;
	}

	/* set clock divisor */
	outp(iobase | UART_LCTL, LCTL_DLAB);
	outp(iobase | UART_DIVLO, DIV_38400 & 0xff);
	outp(iobase | UART_DIVHI, DIV_38400 >> 8);
	/* set format 8n1 */
	outp(iobase | UART_LCTL, LCTL_8N1);
	/* clear and enable fifo */
	outp(iobase | UART_FIFO, FIFO_ENABLE_CLEAR);
	/* assert RTS and DTR */
	outp(iobase | UART_MCTL, MCTL_DTR_RTS_OUT2);
	return 0;
}

static void ser_putchar(int c)
{
	if(c == '\n') {
		ser_putchar('\r');
	}

	while((inp(iobase | UART_LSTAT) & LST_TREG_EMPTY) == 0);
	outp(iobase | UART_DATA, c);
}

static void ser_puts(const char *s)
{
	while(*s) {
		while((inp(iobase | UART_LSTAT) & LST_TREG_EMPTY) == 0);
		if(*s == '\n') {
			outp(iobase | UART_DATA, '\r');
		}
		outp(iobase | UART_DATA, *s++);
	}
}
