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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "logger.h"

int init_logger(const char *fname)
{
	int fd;
	if((fd = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1) {
		fprintf(stderr, "init_logger: failed to open %s: %s\n", fname, strerror(errno));
		return -1;
	}

	close(1);
	close(2);
	dup(fd);
	dup(fd);
	return 0;
}

void stop_logger(void)
{
	fflush(stdout);
	fflush(stderr);
	init_logger("CON");
}
