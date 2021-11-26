S3 Virge driver hack
====================

Hacking the S3 Virge graphics chip, to expose a linear framebuffer, and utilize
its 2D acceleration capabilities (fills, blits, hwcursor). Ideally this would
end up in a portable self-contained "library" for use in DOS projects and kernel
drivers.

Currently runs under DOS, builds with Watcom. Type `wmake` to build.

Hardware docs: http://mutantstargoat.com/~nuclear/tmp/virge.pdf


Copyright (C) 2021 John Tsiombikas <nuclear@member.fsf.org>

This program is free software. Feel free to use, modify, and/or redistribute
copies, under the term of the GNU General Public License v3, or at your option
any later version published by the Free Software Foundation. See COPYING for
details.
