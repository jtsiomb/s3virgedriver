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
#include "virge.h"
#include "s3regs.h"
#include "pci.h"
#include "vbe.h"
#include "mem.h"

#define MMIO_SIZE	0x2000000

#define S3D_BUSY	(!(MMREG_STAT & STAT_S3D_IDLE))

static const char *devtype_name(uint32_t devtype);

static struct pci_device *pcidev;
static volatile void *mmio;
static uint32_t s3d_dfmt;
static uint32_t vmemsize;
static uint32_t devtype;

static unsigned char orig_winctl, orig_memctl1, orig_memcfg;

int s3v_init(void)
{
	if(pcidev) {
		fprintf(stderr, "S3 Virge driver already initialized\n");
		return 0;
	}

	if(!(pcidev = find_pci_dev(PCI_VENDOR_S3, PCI_DEVICE_VIRGE)) ||
			!(devtype = s3v_detect())) {
		if(!(pcidev = find_pci_dev(PCI_VENDOR_S3, PCI_DEVICE_TRIO)) ||
				!(devtype = s3v_detect())) {
			return -1;
		}
	}

	vmemsize = crtc_read(REG_CRTCX_CFG1) & CRTCX_CFG1_2MB ? 2 : 4;
	printf("Found S3 %s %uMB (%x:%x.%x)\n", devtype_name(devtype), vmemsize,
			pcidev->bus, pcidev->dev, pcidev->func);
	fflush(stdout);
	vmemsize <<= 20;

	if(devtype != DEVID_VIRGE) {
		/* XXX */
		fprintf(stderr, "Only Virge is currently supported\n");
		return -1;
	}
	return 0;
}

void *s3v_set_video(int xsz, int ysz, int bpp)
{
	int mode, val;
	uint32_t iobase;

	if(!(mode = vbe_find_mode(xsz, ysz, bpp))) {
		fprintf(stderr, "failed to find suitable video mode (%dx%d %d bpp)\n", xsz, ysz, bpp);
		return 0;
	}
	vbe_set_mode(mode);

	if(mmio) {
		mem_unmap((void*)mmio);
	}

	s3v_extreg_unlock();	/* just in case the modeset re-locked them */

	iobase = pcidev->cfg.base_addr[0] & 0xff000000;

	/* map 32mb from iobase */
	if(!(mmio = mem_map(iobase, MMIO_SIZE))) {
		fprintf(stderr, "failed to map MMIO range (32mb from %08x)\n", iobase);
		return 0;
	}
	printf("Mapped MMIO %08x-%08x (32mb) to virtual %p\n", iobase, iobase + MMIO_SIZE - 1, mmio);
	s3v_mmio_vaddr = (uint32_t)mmio;

	/* enable new-style MMIO */
	orig_memctl1 = crtc_read(REG_CRTCX_MEMCTL1);
	crtc_write(REG_CRTCX_MEMCTL1, CRTCX_MEMCTL1_MMIO_NEW);

	/* make sure enhanced mode is enabled */
	if(!(val = crtc_read(REG_CRTCX_MISC1)) & CRTCX_MISC1_ENH_EN) {
		crtc_write(REG_CRTCX_MISC1, val | CRTCX_MISC1_ENH_EN);
	}
	orig_memcfg = crtc_read(REG_CRTCX_MEMCFG);
	val = (orig_memcfg & ~CRTCX_MEMCFG_BASEOFFS_EN) | CRTCX_MEMCFG_ENHMAP;
	crtc_write(REG_CRTCX_MEMCFG, val);

	while(S3D_BUSY);	/* wait for any pending S3D operations (unlikely) */

	/* enable linear addressing and set window size to 4mb (max) */
	orig_winctl = crtc_read(REG_CRTCX_WINCTL);
	val = orig_winctl & ~CRTCX_WINCTL_SZMASK;
	crtc_write(REG_CRTCX_WINCTL, val | CRTCX_WINCTL_LADDR_EN | CRTCX_WINCTL_4M);

	switch(bpp) {
	case 8:
		s3d_dfmt = S3D_CMD_DFMT8;
		break;
	case 15:
	case 16:
		s3d_dfmt = S3D_CMD_DFMT16;
		break;

	case 24:
	case 32:
		s3d_dfmt = S3D_CMD_DFMT24;
	}

	return (char*)mmio;
}

void s3v_set_text(void)
{
	if(mmio) {
		crtc_write(REG_CRTCX_MEMCTL1, orig_memctl1);
		crtc_write(REG_CRTCX_MEMCFG, orig_memcfg);
		crtc_write(REG_CRTCX_WINCTL, orig_winctl);

		mem_unmap((void*)mmio);
		mmio = 0;
	}
	vbe_set_mode(3);
}

void s3v_fillrect(int x, int y, int w, int h, int color)
{
	MMREG_S3D_FGCOL = color;
	MMREG_S3D_RECTSZ = ((w - 1) << 16) | (h & 0x3ff);
	MMREG_S3D_DSTPOS = (x << 16) | (y & 0x3ff);
	MMREG_S3D_CMD = S3D_CMD_DRAW | S3D_CMD_PATMONO | S3D_CMD_RECT |
		s3d_dfmt | S3D_CMD_LR | S3D_CMD_TB | S3D_CMD_ROP(ROP_PAT);
}

void s3v_imgcopy(uint32_t dest, void *src, int x, int y, int xsz, int ysz, int pitch)
{
	/* TODO */
}

void s3v_cursor_color(int fr, int fg, int fb, int br, int bg, int bb)
{
	crtc_read(REG_CRTCX_CURMODE);	/* reset color stack ptr */
	crtc_write(REG_CRTCX_CURFG, fr);
	crtc_write(REG_CRTCX_CURFG, fg);
	crtc_write(REG_CRTCX_CURFG, fb);
	crtc_read(REG_CRTCX_CURMODE);	/* reset color stack ptr */
	crtc_write(REG_CRTCX_CURBG, br);
	crtc_write(REG_CRTCX_CURBG, bg);
	crtc_write(REG_CRTCX_CURBG, bb);
}

void s3v_cursordef(int w, int h, void *img, void *mask, int hotx, int hoty)
{
	int i, j;
	uint32_t offs = vmemsize - 1024;
	uint16_t *dest = (uint16_t*)((char*)mmio + offs);
	uint16_t val, *iptr = img, *mptr = mask;

	for(i=0; i<h; i++) {
		for(j=0; j<4; j++) {
			if(j < (w >> 4)) {
				val = *mptr++;
				*dest++ = (val >> 8) | (val << 8);
				val = *iptr++;
				*dest++ = (val >> 8) | (val << 8);
			} else {
				*dest++ = 0;
				*dest++ = 0;
			}
		}
	}
	if(h < 64) memset(dest, 0, (64 - h) << 4);

	offs >>= 10;
	crtc_write(REG_CRTCX_CURADDR_H, offs >> 8);
	crtc_write(REG_CRTCX_CURADDR_L, offs & 0xff);

	crtc_write(REG_CRTCX_CURXOFFS, hotx);
	crtc_write(REG_CRTCX_CURYOFFS, hoty);

	crtc_wrmask(REG_CRTCX_DACCTL, CRTCX_DACCTL_CURX11, CRTCX_DACCTL_CURX11);
}

void s3v_cursordef_rgba(int w, int h, uint32_t *img, int hotx, int hoty)
{
	int i, j;
	uint32_t offs = vmemsize - 1024;
	uint16_t *dest = (uint16_t*)((char*)mmio + offs);
	uint32_t pix, fgcol = 0, bgcol = 0;
	uint16_t bits = 0, mask = 0;

	for(i=0; i<h; i++) {
		for(j=0; j<64; j++) {
			pix = j < w ? *img++ : 0;
			if(!(pix & 0xff000000)) {
				bits <<= 1;
				mask <<= 1;
			} else {
				if(bgcol == 0) {
					bgcol = pix;
				} else if(fgcol == 0) {
					fgcol = pix;
				}
				bits = (bits << 1) | (pix == fgcol ? 1 : 0);
				mask = (mask << 1) | 1;
			}
			if((j & 0xf) == 0xf) {
				*dest++ = mask;
				*dest++ = bits;
			}
		}
	}
	if(h < 64) memset(dest, 0, (64 - h) << 4);

	offs >>= 10;
	crtc_write(REG_CRTCX_CURADDR_H, offs >> 8);
	crtc_write(REG_CRTCX_CURADDR_L, offs & 0xff);

	crtc_write(REG_CRTCX_CURXOFFS, hotx);
	crtc_write(REG_CRTCX_CURYOFFS, hoty);

	s3v_cursor_color(fgcol >> 16, fgcol >> 8, fgcol, bgcol >> 16, bgcol >> 8, bgcol);

	crtc_wrmask(REG_CRTCX_DACCTL, CRTCX_DACCTL_CURX11, CRTCX_DACCTL_CURX11);
}

void s3v_showcursor(int show)
{
	if(show) {
		crtc_wrmask(REG_CRTCX_CURMODE, CRTCX_CURMODE_EN, CRTCX_CURMODE_EN);
	} else {
		crtc_write(REG_CRTCX_CURMODE, 0);
	}
}

void s3v_cursor(int x, int y)
{
	crtc_write(REG_CRTCX_CURX_H, (x >> 8) & 7);
	crtc_write(REG_CRTCX_CURX_L, x & 0xff);
	crtc_write(REG_CRTCX_CURY_H, (y >> 8) & 7);
	crtc_write(REG_CRTCX_CURY_L, y & 0xff);
}

void s3v_extreg_unlock(void)
{
	unsigned char cur;

	sc_write(REG_SCX_UNLOCK, 0x06);			/* unlock ext. SC regs SR9-SRff */
	crtc_write(REG_CRTCX_UNLOCK1, 0x48);	/* unlock ext. CRTC regs CR2d-CR3f */
	crtc_write(REG_CRTCX_UNLOCK2, 0xa5);	/* unlock ext. CRTC regs CR40-CRff */
	/* enable access to enhanced interface */
	cur = crtc_read(REG_CRTCX_SYSCONF);
	outp(CRTC_DATA_PORT, cur | CRTCX_SYSCONF_ENH_EN);
}

static unsigned int supdev[] = {
	DEVID_TRIO32,
	DEVID_TRIO64,
	DEVID_VIRGE
};

int s3v_detect(void)
{
	int i;
	uint32_t devid, rev, chipid;
	s3v_extreg_unlock();
	devid = crtc_read(REG_CRTCX_DEVID_L);
	devid |= (uint32_t)crtc_read(REG_CRTCX_DEVID_H) << 8;
	rev = crtc_read(REG_CRTCX_REV);
	chipid = crtc_read(REG_CRTCX_CHIPID);

	printf("S3 dev %x rev %x, chip %x stepping %x\n", devid, rev, chipid & 0xf,
			chipid >> 4);

	for(i=0; supdev[i]; i++) {
		if(supdev[i] == devid) {
			return devid;
		}
	}
	return 0;
}

static const char *devtype_name(uint32_t devtype)
{
	switch(devtype) {
	case DEVID_VIRGE:
		return "Virge";
	case DEVID_TRIO32:
		return "Trio32";
	case DEVID_TRIO64:
		return "Trio64";
	}
	return "unknown";
}
