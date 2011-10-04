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
#include <string.h>
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
cmd_create(create_args_t *args)
{
	void *k_addr = MAP_FAILED, *b_addr = MAP_FAILED, *r_addr = MAP_FAILED;
	int k_fd = -1, b_fd = -1, r_rd = -1;
	struct stat k_stat, b_stat, r_stat;
	image_create_args_t components;
	int rv = 1;

	memset(&k_stat, 0, sizeof(struct stat));
	memset(&b_stat, 0, sizeof(struct stat));
	memset(&r_stat, 0, sizeof(struct stat));
	memset(&components, 0, sizeof(image_create_args_t));

	k_fd = open(args->kernel, O_RDONLY);
	if (-1 == k_fd) {
		perror("Failed to open kernel file");
		return 1;
	}
	if (0 != fstat(k_fd, &k_stat)) {
		perror("Failed to read kernel stat");
		goto create_fail;
	}
	k_addr = mmap(NULL, k_stat.st_size, PROT_READ, MAP_PRIVATE, k_fd, 0);
	if (MAP_FAILED == k_addr) {
		perror("Failed to memory map kernel file");
		goto create_fail;
	}
	components.kernel = k_addr;
	components.kernel_len = k_stat.st_size;

	if (args->bcode) {
		b_fd = open(args->bcode, O_RDONLY);
		if (-1 == b_fd) {
			perror("Failed to open bootcode file");
			goto create_fail;
		}
		if (0 != fstat(b_fd, &b_stat)) {
			perror("Failed to read bootcode stat");
			goto create_fail;
		}
		b_addr = mmap(NULL, b_stat.st_size, PROT_READ, MAP_PRIVATE, b_fd, 0);
		if (MAP_FAILED == b_addr) {
			perror("Failed to memory map bootcode file");
			goto create_fail;
		}
		components.bcode = b_addr;
		components.bcode_len = b_stat.st_size;
	}

	if (args->ramdisk) {
		r_rd = open(args->ramdisk, O_RDONLY);
		if (-1 == r_rd) {
			perror("Failed to open ramdisk file");
			goto create_fail;
		}
		if (0 != fstat(r_rd, &r_stat)) {
			perror("Failed to read ramdisk stat");
			goto create_fail;
		}
		r_addr = mmap(NULL, r_stat.st_size, PROT_READ, MAP_PRIVATE, r_rd, 0);
		if (MAP_FAILED == r_addr) {
			perror("Failed to memory map ramdisk file");
			goto create_fail;
		}

		components.ramdisk = r_addr;
		components.ramdisk_len = r_stat.st_size;
	}

	components.use_zlib = args->use_zlib;
	components.load_offset = args->load_offset;
	components.image_descr = args->image_descr;
	components.image_version = args->image_version;

	rv = boost_create(args->outfile, &components);

create_fail:
	if (MAP_FAILED != k_addr) {
		if (-1 == munmap(k_addr, k_stat.st_size)) {
			perror("Failed to unmap kernel from memory");
		}
	}
	if (MAP_FAILED != b_addr) {
		if (-1 == munmap(b_addr, b_stat.st_size)) {
			perror("Failed to unmap bootcode from memory");
		}
	}
	if (MAP_FAILED != r_addr) {
		if (-1 == munmap(r_addr, r_stat.st_size)) {
			perror("Failed to unmap ramdisk from memory");
		}
	}

	if (-1 != k_fd) {
		if (0 != close(k_fd)) {
			perror("Failed to close kernel file");
		}
	}
	if (-1 != b_fd) {
		if (0 != close(b_fd)) {
			perror("Failed to close bootcode file");
		}
	}
	if (-1 != r_rd) {
		if (0 != close(r_rd)) {
			perror("Failed to close ramdisk file");
		}
	}

	return rv;
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

	memset(&f_stat, 0, sizeof(struct stat));

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

	memset(&f_stat, 0, sizeof(struct stat));

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
