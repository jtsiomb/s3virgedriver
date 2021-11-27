#include <i86.h>
#include <dos.h>
#include "mouse.h"

int reset_mouse(void)
{
	union REGS regs = {0};

	if(!_dos_getvect(0x33)) {
		return -1;
	}
	
	int386(0x33, &regs, &regs);
	return regs.w.ax == 0xffff ? 0 : -1;
}

unsigned int read_mouse(int *xp, int *yp)
{
	union REGS regs = {0};

	regs.w.ax = 3;
	int386(0x33, &regs, &regs);

	*xp = regs.w.cx;
	*yp = regs.w.dx;
	return regs.w.bx;
}

void set_mouse_xrange(int xmin, int xmax)
{
	union REGS regs = {0};
	regs.w.ax = 7;
	regs.w.cx = xmin;
	regs.w.dx = xmax;
	int386(0x33, &regs, &regs);
}

void set_mouse_yrange(int ymin, int ymax)
{
	union REGS regs = {0};
	regs.w.ax = 8;
	regs.w.cx = ymin;
	regs.w.dx = ymax;
	int386(0x33, &regs, &regs);
}
