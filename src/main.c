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
#include <time.h>
#include <conio.h>
#include "pci.h"
#include "virge.h"
#include "logger.h"
#include "mouse.h"
#include "inttypes.h"

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
static const char *opt_logfile;

enum {OPT_VSYNC = 1, OPT_BLIT = 2, OPT_DMA = 4, OPT_BENCH = 8};
static unsigned int opt;

static int mx, my;
static int dirty = 1;

static uint16_t *backbuf;

int main(int argc, char **argv)
{
	int i, j;
#if BPP == 8
	unsigned char *fbptr;
#else
	uint16_t *fbptr;
#endif
	time_t t0, tsec;
	unsigned long nframes = 0;

	opt_logfile = getenv("LOGFILE");
	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	if(init_logger(opt_logfile ? opt_logfile : "virge.log") == -1) {
		return 1;
	}

	if(init_pci() == -1) {
		logmsg("failed to access the PCI bus\n");
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

	backbuf = malloc(opt_width * opt_height * (BPP / 8));
	fbptr = backbuf;
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

	t0 = time(0);
	for(;;) {
		if(kbhit()) {
			switch(getch()) {
			case 27:
				goto end;
			case ' ':
				if(!(opt & OPT_BENCH)) {
					opt |= OPT_BENCH;
					t0 = time(0);
					dirty = 1;
				}
				break;
			}
		}
		if(have_mouse) {
			read_mouse(&mx, &my);
		}

		if(opt & OPT_VSYNC) {
			wait_vsync();
		}
		s3v_cursor(mx, my);

		if(!dirty) continue;
		if(!(opt & OPT_BENCH)) dirty = 0;

		if(opt & OPT_BLIT) {
			if(opt & OPT_DMA) {
				s3v_dmacopy(240, 180, backbuf, 0, 0, 160, 120, opt_width * BPP >> 3);
				s3v_cmdfifo_finish();	/* XXX */
			} else {
				s3v_imgcopy(240, 180, backbuf, 0, 0, 160, 120, opt_width * BPP >> 3);
			}
		} else {
			s3v_s3dfifo_finish();
			memcpy(fb, backbuf, opt_width * opt_height * BPP >> 3);
		}

		/*
#if BPP == 8
		s3v_fillrect(opt_width >> 4, opt_height >> 3, opt_width >> 1, opt_height >> 1, 6);
#else
		s3v_fillrect(opt_width >> 4, opt_height >> 3, opt_width >> 1, opt_height >> 1, 0x0700);
#endif
		*/
		nframes++;
	}
end:
	tsec = time(0) - t0;

	stop_logger();
	s3v_set_text();

	if(opt & OPT_BENCH) {
		printf("framerate: %.2f fps\n", (float)nframes / (float)tsec);
	}
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
	" -cpublit			copy frame with memcpy\n"
	" -pioblit			copy frame with PIO blit\n"
	" -dmablit       	copy frame with DMA blit\n"
	" -log <file|COMn>  logfile output filename or COM port\n"
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
				opt |= OPT_VSYNC;
			} else if(strcmp(argv[i], "-novsync") == 0) {
				opt &= ~OPT_VSYNC;
			} else if(strcmp(argv[i], "-cpublit") == 0) {
				opt &= ~(OPT_BLIT | OPT_DMA);
			} else if(strcmp(argv[i], "-pioblit") == 0) {
				opt |= OPT_BLIT;
				opt &= ~OPT_DMA;
			} else if(strcmp(argv[i], "-dmablit") == 0) {
				opt |= OPT_BLIT | OPT_DMA;
			} else if(strcmp(argv[i], "-log") == 0) {
				if(!argv[++i]) {
					fprintf(stderr, "-log should be followed by a filename, COM1, or COM2\n");
					return -1;
				}
				opt_logfile = argv[i];
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
