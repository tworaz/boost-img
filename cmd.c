/*-
 * Copyright (c) 2011 Peter Tworek
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "boost.h"
#include "cmd.h"


int
cmd_info(const char *filename)
{
	boost_hdr_t hdr;
	ssize_t len;
	int fd;

	fd = open(filename, O_RDONLY);
	if (-1 == fd) {
		perror("Failed to open image file");
		return 1;
	}

	len = read(fd, &hdr, sizeof(boost_hdr_t));
	if (len != sizeof(boost_hdr_t)) {
		fprintf(stderr, "Failed to read BooSt header!\n");
		goto info_fail;
	}

	boost_print_info(hdr);

info_fail:
	if (0 != close(fd)) {
		perror("Failed to close image file");
		return 1;
	}

	return 0;
}

int
cmd_create(const char *outfile, const char *kernel, const char *initrd,
           const char *bootstrap, bool use_zlib)
{
	printf("TODO: implement %s\n", __func__);
	return 1;
}

int
cmd_extract(const char *filename)
{
	struct stat f_stat;
	boost_hdr_t hdr;
	ssize_t len;
	void *addr = NULL;
	int fd;
	int rv = 0;

	fd = open(filename, O_RDONLY);
	if (-1 == fd) {
		perror("Failed to open image file");
		return 1;
	}

	len = read(fd, &hdr, sizeof(boost_hdr_t));
	if (len != sizeof(boost_hdr_t)) {
		fprintf(stderr, "Failed to read BooSt header!\n");
		goto extract_fail;
	}

	if (0 != fstat(fd, &f_stat)) {
		perror("Failed to read image stat");
		goto extract_fail;
	}

	addr = mmap(NULL, f_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (MAP_FAILED == addr) {
		perror("Failed to memory map image");
		goto extract_fail;
	}

	rv = boost_extract(hdr, addr + sizeof(boost_hdr_t));

extract_fail:
	if (MAP_FAILED != addr) {
		if (-1 == munmap(addr, hdr.image_size)) {
			perror("Failed to unmap image from memory");
			rv = 1;
		}
	}

	if (0 != close(fd)) {
		perror("Failed to close image file");
		return 1;
	}

	return rv;
}

int
cmd_check(const char *filename)
{
	struct stat f_stat;
	boost_hdr_t hdr;
	ssize_t len;
	void *addr = NULL;
	int fd;
	int rv = 0;

	fd = open(filename, O_RDONLY);
	if (-1 == fd) {
		perror("Failed to open image file");
		return 1;
	}

	len = read(fd, &hdr, sizeof(boost_hdr_t));
	if (len != sizeof(boost_hdr_t)) {
		fprintf(stderr, "Failed to read BooSt header!\n");
		goto check_fail;
	}

	if (0 != fstat(fd, &f_stat)) {
		perror("Failed to read image stat");
		goto check_fail;
	}

	addr = mmap(NULL, f_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (MAP_FAILED == addr) {
		perror("Failed to memory map image");
		goto check_fail;
	}

	rv = boost_check(hdr, addr + sizeof(boost_hdr_t));

check_fail:
	if (MAP_FAILED != addr) {
		if (-1 == munmap(addr, hdr.image_size)) {
			perror("Failed to unmap image from memory");
			rv = 1;
		}
	}

	if (0 != close(fd)) {
		perror("Failed to close image file");
		return 1;
	}

	return rv;
}
