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
#include "mem.h"


#if defined(__MSDOS__) || defined(MSDOS)
void *mem_map(uint32_t paddr, uint32_t size)
{
	uint32_t vaddr;
	union REGS regs = {0};

	regs.w.ax = 0x800;	/* DPMI physical address mapping call */
	regs.w.bx = paddr >> 16;
	regs.w.cx = paddr;
	regs.w.si = size >> 16;
	regs.w.di = size;
	int386(0x31, &regs, &regs);

	if(regs.w.cflag) {
		return 0;
	}

	vaddr = ((uint32_t)regs.w.bx << 16) | (uint32_t)regs.w.cx;
	return (void*)vaddr;
}

void mem_unmap(void *ptr)
{
	union REGS regs = {0};
	uint32_t vaddr = (uint32_t)ptr;

	regs.w.ax = 0x801;	/* DPMI free physical address mapping call */
	regs.w.bx = vaddr >> 16;
	regs.w.cx = vaddr;
	int386(0x31, &regs, &regs);
}
#endif
