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
#include "virge.h"
#include "s3regs.h"
#include "pci.h"
#include "vbe.h"
#include "mem.h"

#define PCI_VENDOR_S3		0x5333
#define PCI_DEVICE_VIRGE	0x5631

#define MMIO_SIZE	0x2000000

#define S3D_BUSY	(!(MMREG_STAT & STAT_S3D_IDLE))

static struct pci_device *pcidev;
static volatile void *mmio;

static unsigned char orig_winctl, orig_memctl1, orig_memcfg;

int s3v_init(void)
{
	if(pcidev) {
		fprintf(stderr, "S3 Virge driver already initialized\n");
		return 0;
	}

	if(!(pcidev = find_pci_dev(PCI_VENDOR_S3, PCI_DEVICE_VIRGE)) || !s3v_detect()) {
		return -1;
	}

	printf("Found S3 Virge (%x:%x.%x)\n", pcidev->bus, pcidev->dev, pcidev->func);
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
	MMREG_S3D_FGCOL = color & 0xff;
	MMREG_S3D_RECTSZ = ((w - 1) << 16) | (h & 0x3ff);
	MMREG_S3D_DSTPOS = (x << 16) | (y & 0x3ff);
	MMREG_S3D_CMD = S3D_CMD_DRAW | S3D_CMD_PATMONO | S3D_CMD_RECT |
		S3D_CMD_LR | S3D_CMD_TB | S3D_CMD_ROP(ROP_PAT);
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

int s3v_detect(void)
{
	unsigned int devid, rev, chipid;
	s3v_extreg_unlock();
	devid = crtc_read(REG_CRTCX_DEVID_L);
	devid |= (unsigned int)crtc_read(REG_CRTCX_DEVID_H) << 8;
	rev = crtc_read(REG_CRTCX_REV);
	chipid = crtc_read(REG_CRTCX_CHIPID);

	printf("S3 dev %x rev %x, chip %x stepping %x\n", devid, rev, chipid & 0xf,
			chipid >> 4);

	return (devid & 0xff) == 0x31;
}
