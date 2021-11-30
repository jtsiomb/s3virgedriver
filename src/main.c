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
#include <string.h>
#include <conio.h>
#include "pci.h"
#include "virge.h"
#include "logger.h"
#include "mouse.h"

#define BPP		16

void wait_vsync(void);
int parse_args(int argc, char **argv);

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

static int opt_width = 640;
static int opt_height = 480;
static int opt_vsync = 1;
static int opt_dma = 0;

static int mx, my;


int main(int argc, char **argv)
{
	int i, j;
#if BPP == 8
	unsigned char *fbptr;
#else
	uint16_t *fbptr;
#endif

	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	init_logger("virge.log");

	if(init_pci() == -1) {
		fprintf(stderr, "failed to access the PCI bus\n");
	}

	if(s3v_init() == -1) {
		stop_logger();
		fprintf(stderr, "no compatible graphics card detected\n");
		return 1;
	}

	if(!(fb = s3v_set_video(opt_width, opt_height, BPP))) {
		stop_logger();
		fprintf(stderr, "failed to set video mode\n");
		return 1;
	}

	if(reset_mouse() != -1) {
		have_mouse = 1;
		set_mouse_xrange(0, opt_width);
		set_mouse_yrange(0, opt_height);
	}


	if(have_mouse) {
		s3v_cursor_color(0, 0, 0, 0xff, 0xff, 0xff);
		s3v_cursordef(CURIMG_W, CURIMG_H, curimg, curmask, 0, 0);
		s3v_showcursor(1);
	}

	for(;;) {
		if(kbhit() && getch() == 27) {
			break;
		}
		if(have_mouse) {
			read_mouse(&mx, &my);
		}

		if(opt_vsync) {
			wait_vsync();
		}
		s3v_cursor(mx, my);

		fbptr = fb;
		for(i=0; i<opt_height; i++) {
			for(j=0; j<opt_width; j++) {
				int chess = ((i >> 5) & 1) == ((j >> 5) & 1);
#if BPP == 8
				*fbptr++ = chess ? 3 : 4;
#else
				*fbptr++ = chess ? 0xf800 : 0x001f;
#endif
			}
		}

#if BPP == 8
		s3v_fillrect(opt_width >> 4, opt_height >> 3, opt_width >> 1, opt_height >> 1, 6);
#else
		s3v_fillrect(opt_width >> 4, opt_height >> 3, opt_width >> 1, opt_height >> 1, 0x0700);
#endif
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

static const char *usage_fmt = "Usage: %s [opt]\n"
	"Options:\n"
	" -s,-size <WxH>    select video resolution\n"
	" -vsync/-novsync   enable/disable vsync\n"
	" -dma/-nodma       enable/disable DMA image copies\n"
	" -h,-help          print usage and exit\n";

int parse_args(int argc, char **argv)
{
	int i;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "-size") == 0) {
				if(!argv[++i] || sscanf(argv[i], "%dx%d", &opt_width, &opt_height) != 2) {
					fprintf(stderr, "%s should be followed by a resoltion (WxH)\n", argv[i - 1]);
					return -1;
				}
			} else if(strcmp(argv[i], "-vsync") == 0) {
				opt_vsync = 1;
			} else if(strcmp(argv[i], "-novsync") == 0) {
				opt_vsync = 0;
			} else if(strcmp(argv[i], "-dma") == 0) {
				opt_dma = 1;
			} else if(strcmp(argv[i], "-nodma") == 0) {
				opt_dma = 0;
			} else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
				printf(usage_fmt, argv[0]);
				exit(0);
			} else {
				fprintf(stderr, "invalid option: %s\n", argv[i]);
				fprintf(stderr, usage_fmt, argv[0]);
				return -1;
			}
		} else {
			fprintf(stderr, "unexpected argument: %s\n", argv[i]);
			fprintf(stderr, usage_fmt, argv[0]);
			return -1;
		}
	}

	return 0;
}
