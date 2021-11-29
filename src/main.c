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
#include "mouse.h"

#define BPP		16

static void *fb;
static int have_mouse;

#define CURIMG_W	16
#define CURIMG_H	16
static uint16_t curimg[] = {
	0x0000,	0x4000, 0x6000, 0x7000, 0x7800, 0x7c00, 0x7e00, 0x7f00,
	0x7f80, 0x7c00, 0x6c00, 0x4600, 0x0600, 0x0300, 0x0300, 0x0000
};
static uint16_t curmask[] = {
	0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00, 0xff80,
	0xffc0, 0xffc0, 0xfe00, 0xef00, 0xcf00, 0x0780, 0x0780, 0x0300
};

void wait_vsync(void);

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
		stop_logger();
		fprintf(stderr, "no compatible graphics card detected\n");
		return 1;
	}

	if(!(fb = s3v_set_video(640, 480, BPP))) {
		stop_logger();
		fprintf(stderr, "failed to set video mode\n");
		return 1;
	}

	if(reset_mouse() != -1) {
		have_mouse = 1;
		set_mouse_xrange(0, 640);
		set_mouse_yrange(0, 480);
	}

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

#if BPP == 8
	s3v_fillrect(50, 50, 320, 240, 6);
#else
	s3v_fillrect(50, 50, 320, 240, 0x0700);
#endif

	if(have_mouse) {
		s3v_cursor_color(0, 0, 0, 0xff, 0xff, 0xff);
		s3v_cursordef(CURIMG_W, CURIMG_H, curimg, curmask, 0, 0);
		s3v_showcursor(1);
	}

	for(;;) {
		if(kbhit() && getch() == 27) {
			break;
		}

		wait_vsync();

		if(have_mouse) {
			int mx, my;
			read_mouse(&mx, &my);
			s3v_cursor(mx, my);
		}
	}

	stop_logger();
	s3v_set_text();
	return 0;
}

#pragma aux wait_vsync = \
	"mov dx, 0x3da" \
	"l1:" \
	"in al, dx" \
	"and al, 0x8" \
	"jnz l1" \
	"l2:" \
	"in al, dx" \
	"and al, 0x8" \
	"jz l2" \
	modify[al dx];
