/*
 * Copyright (c) 2016, Tom G., <roboter972@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Created:		29.12.2016
 * Last modified:	30.12.2016
 *
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/bootimg.h"

/*
 +------------------------------------------------------------------+
 |			Boot image layout:			    |
 +------------------------------------------------------------------+
 | 	Name			|		Size (Pages)	    |
 +------------------------------------------------------------------+
 | 	Boot header 		| 		1    		    |
 +------------------------------------------------------------------+
 |	Kernel			|  (kernel_size + page_size - 1) /  |
 |				|		page_size	    |
 +------------------------------------------------------------------+
 | 	Ramdisk			|  (ramdisk_size + page_size - 1) / |
 |				|		page_size	    |
 +------------------------------------------------------------------+
 |		              .....				    |
 +------------------------------------------------------------------+
 |			Boot image header layout:		    |
 +------------------------------------------------------------------+
 | 	Name			|		Size (Bytes) 	    |
 +------------------------------------------------------------------+
 | 	magic			|		8	     	    |
 +------------------------------------------------------------------+
 | 	kernel_size		|		4	     	    |
 +------------------------------------------------------------------+
 | 	kernel_addr		|		4	     	    |
 +------------------------------------------------------------------+
 | 	ramdisk_size		|		4	     	    |
 +------------------------------------------------------------------+
 | 	ramdisk_addr		|		4	     	    |
 +------------------------------------------------------------------+
 | 	second_size		|		4	     	    |
 +------------------------------------------------------------------+
 | 	second_addr		|		4	     	    |
 +------------------------------------------------------------------+
 | 	tags_addr		|		4	     	    |
 +------------------------------------------------------------------+
 | 	page_size		|		4	     	    |
 +------------------------------------------------------------------+
 | 	dt_size			|		4	     	    |
 +------------------------------------------------------------------+
 |	os_version		|		4		    |
 +------------------------------------------------------------------+
 | 	name  			|		16	     	    |
 +------------------------------------------------------------------+
 | 	cmdline			|		512	     	    |
 +------------------------------------------------------------------+
 | 	id			|		32	     	    |
 +------------------------------------------------------------------+
 |	extra_cmdline		|		1024		    |
 +------------------------------------------------------------------+
 */

#define CHUNKSZ(size)	(size)

int main(int argc, char **argv) {
	int inf, outf, tbytes, wrbytes, rbytes;
	int kernelsz, rdsz, pagesz, bufsz = 0, tmp, remsz;
	char magic[BOOT_MAGIC_SIZE + 1] = { '\0' };
	char *buf;
	off_t off;

	if (argc != 2) {
		fprintf(stderr, "Usage: extrd <boot.img>\n");
		return EXIT_FAILURE;
	}

	if ((inf = open(argv[1], O_RDONLY)) < 0) {
		perror("open()");
		return EXIT_FAILURE;
	}

	if ((off = read(inf, magic, BOOT_MAGIC_SIZE)) != BOOT_MAGIC_SIZE) {
		fprintf(stderr, "Supplied file no valid android boot image!\n");
		goto out2;
	}

	if (!memcmp(magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		fprintf(stdout, "Magic match found!\n");
	} else {
		fprintf(stderr, "Supplied file no valid android boot image!\n");
		goto out2;
	}

	/* lseek shall not fail */
	(void) lseek(inf, off, SEEK_SET);
	if ((rbytes = read(inf, &kernelsz, 4)) != 4) {
		perror("read()");
		goto out2;
	}

	(void) lseek(inf, off + 8, SEEK_SET);
	if ((rbytes = read(inf, &rdsz, 4)) != 4) {
		perror("read()");
		goto out2;
	}

	(void) lseek(inf, off + 28, SEEK_SET);
	if ((rbytes = read(inf, &pagesz, 4)) != 4) {
		perror("read()");
		goto out2;
	}

	fprintf(stdout, "Kernelsize\t\t=\t%8d Bytes\nRamdisksize\t\t=\t%8d Bytes\nPagesize\t\t=\t%8d Bytes\n", kernelsz, rdsz, pagesz);

	if ((outf = open("ramdisk.gz", O_CREAT | O_WRONLY | O_TRUNC, 0664)) < 0) {
		perror("open()");
		goto out2;
	}

	if (!(buf = calloc(CHUNKSZ(pagesz) + 1, sizeof(char))))
		goto out;

	off = ((kernelsz + pagesz - 1) / pagesz) * pagesz + pagesz;
	(void) lseek(inf, off, SEEK_SET);

	remsz = rdsz - pagesz;
	for (tmp = rdsz; tmp > remsz; tmp --) {
		if ((tmp & (pagesz - 1)) == 0) {
			bufsz = tmp;
			break;
		}
	}

	if (bufsz == 0) {
		fprintf(stderr, "Error: Could not determine upper chunksize aligned read-limit!\n");
		goto out;
	}

	/* Read from inf in pagesz chunks till bufsz */
	for (tbytes = 0; tbytes < bufsz; ) {
		if ((rbytes = read(inf, buf, CHUNKSZ(pagesz))) != CHUNKSZ(pagesz)) {
			perror("read()");
			goto out;
		}
		if ((wrbytes = write(outf, buf, rbytes)) != rbytes) {
			perror("write()");
			goto out;
		}
		tbytes += wrbytes;
	}

	/* Read the last chunk (rdsz -bufsz bytes) */
	if ((rbytes = read(inf, buf, remsz = rdsz - bufsz)) != remsz) {
		perror("read()");
		goto out;
	}

	if ((wrbytes = write(outf, buf, rbytes)) != rbytes) {
		perror("write()");
		goto out;
	}

	fprintf(stdout, "Total Bytes written\t=\t%8d Bytes\n", tbytes += wrbytes);

	close(inf);
	close(outf);
	free(buf);

	return EXIT_SUCCESS;
out:
	free(buf);
	close(outf);
out2:
	close(inf);

	return EXIT_FAILURE;
}
