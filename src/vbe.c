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
#include <i86.h>
#include "vbe.h"

static struct vbemode {
	int mode;
	int xsz, ysz, bpp;
} stdmodes[] = {
	{0x100, 640, 400, 8},
	{0x101, 640, 480, 8},
	{0x102, 800, 600, 4},
	{0x103, 800, 600, 8},
	{0x104, 1024, 768, 4},
	{0x105, 1024, 768, 8},
	{0x106, 1280, 1024, 4},
	{0x107, 1280, 1024, 8},
	{0x10d, 320, 200, 15},
	{0x10e, 320, 200, 16},
	{0x10f, 320, 200, 24},
	{0x110, 640, 480, 15},
	{0x111, 640, 480, 16},
	{0x112, 640, 480, 24},
	{0x113, 800, 600, 15},
	{0x114, 800, 600, 16},
	{0x115, 800, 600, 24},
	{0x116, 1024, 768, 15},
	{0x117, 1024, 768, 16},
	{0x118, 1024, 768, 24},
	{0x119, 1280, 1024, 15},
	{0x11a, 1280, 1024, 16},
	{0x11b, 1280, 1024, 24}
};
#define NUM_STDMODES	(sizeof stdmodes / sizeof *stdmodes)

int vbe_find_mode(int xsz, int ysz, int bpp)
{
	int i;
	struct vbemode *mode;

	mode = stdmodes;
	for(i=0; i<NUM_STDMODES; i++) {
		if(mode->xsz == xsz && mode->ysz == ysz && mode->bpp == bpp) {
			return mode->mode;
		}
		mode++;
	}
	return 0;
}

int vbe_set_mode(int mode)
{
	union REGS regs = {0};

	if(mode < 0x100) {
		regs.w.ax = mode;
	} else {
		regs.w.ax = 0x4f02;
		regs.w.bx = mode;
	}
	int386(0x10, &regs, &regs);

	if(regs.w.ax != 0x4f00) {
		return -1;
	}
	return 0;
}
