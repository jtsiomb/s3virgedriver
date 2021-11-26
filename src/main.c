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
#include <stdlib.h>
#include <conio.h>
#include "pci.h"
#include "virge.h"
#include "logger.h"

#define BPP		8

static void *fb;

int main(void)
{
	int i, j;
#if BPP == 8
	unsigned char *fbptr;
#else
	uint16_t *fbptr;
#endif

	init_logger("virge.log");

	if(init_pci() == -1) {
		stop_logger();
		fprintf(stderr, "failed to access the PCI bus\n");
		return 1;
	}

	if(s3v_init() == -1) {
		return 1;
	}

	if(!(fb = s3v_set_video(640, 480, BPP))) {
		stop_logger();
		fprintf(stderr, "failed to set video mode\n");
		return 1;
	}
	printf("fb addr: %p\n", fb);

	fbptr = fb;
	for(i=0; i<480; i++) {
		for(j=0; j<640; j++) {
			int chess = ((i >> 5) & 1) == ((j >> 5) & 1);
#if BPP == 8
			*fbptr++ = chess ? 3 : 4;
#else
			*fbptr++ = chess ? 0xf800 : 0x001f;
#endif
		}
	}

	s3v_fillrect(50, 50, 320, 240, 6);

	for(;;) {
		if(kbhit() && getch() == 27) {
			break;
		}
	}

	stop_logger();
	s3v_set_text();
	return 0;
}
