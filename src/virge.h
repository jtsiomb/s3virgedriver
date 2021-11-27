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
#ifndef VIRGE_H_
#define VIRGE_H_

#include "inttypes.h"

int s3v_init(void);

void *s3v_set_video(int xsz, int ysz, int bpp);
void s3v_set_text(void);

void s3v_fillrect(int x, int y, int w, int h, int color);

void s3v_cursor_color(int fr, int fg, int fb, int br, int bg, int bb);
void s3v_cursordef(int w, int h, void *img, void *mask, int hotx, int hoty);
void s3v_cursordef_rgba(int w, int h, uint32_t *img, int hotx, int hoty);
void s3v_showcursor(int show);
void s3v_cursor(int x, int y);

void s3v_extreg_unlock(void);
int s3v_detect(void);


#ifdef __WATCOMC__
void s3v_setpal(short idx, unsigned char r, unsigned char g, unsigned char b);
#pragma aux s3v_setpal = \
	"test ax, 0x8000" \
	"jnz skip_dacaddr" \
	"mov dx, 0x3c8" \
	"out dx, al" \
	"skip_dacaddr:" \
	"mov dx, 0x3c9" \
	"mov al, bl" \
	"shr al, 2" \
	"out dx, al" \
	"mov al, bh" \
	"shr al, 2" \
	"out dx, al" \
	"mov al, cl" \
	"shr al, 2" \
	"out dx, al" \
	parm[ax][bl][bh][cl] \
	modify[dx];
#endif /* __WATCOMC__ */

#endif	/* VIRGE_H_ */
