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

#ifndef _BOOST_H_
#define _BOOST_H_

#include <stdint.h>

/* RAM image */
#define BOOST_FLAG_RAM_IMG	(1<<0)
/* Store without header */
#define BOOST_FLAG_NO_HDR	(1<<1)
/* Erase target region before saving */
#define BOOST_FLAG_ERASE	(1<<2)
/* No reset before starting the image */
#define BOOST_FLAG_NO_RST	(1<<3)
/* Copy to to OS partition */
#define BOOST_FLAG_COPY_OS	(1<<4)
/* Image is compressed with zlib */
#define BOOST_FLAG_ZLIB		(1<<16)
/* Image includes splash screen */
#define BOOST_FLAG_SPLASH	(1<<17)

/* Offset at which actual image data begins. */
#define BOOST_IMAGE_DATA_OFFSET	260
/* Number of bytes used to calculate header checksum. */
#define BOOST_HEADER_CRC_BYTES 252

/* Initial legacy bootcode instruction starts at this offset. */
#define LEGACY_BCODE_START_OFFSET	408
/* Size of boot code used in original Psion images */
#define LEGACY_BCODE_SIZE		1052

/*
 * First four bytes of the new boot code contain some usefull info.
 * In general it looks like 0xMMMMMVVV, where
 * - MMMMM is the MAGIC number identifying the bootcode.
 * - VVV is the version number.
 */
#define BCODE_MAGIC 		0xBC0DE000
#define BCODE_MAGIC_MASK	0xfffff000
#define BCODE_VERSION_MASK	0x00000fff

#define STARTUP_BYTES		16

typedef struct boost_header
{
	uint32_t	branch_offset;
	uint32_t	unknown_1;
	uint32_t	image_id;
	uint32_t	platform_id;
	uint32_t	image_size;
	uint32_t	image_checksum;
	uint32_t	load_offset;
	uint32_t	flags;
	char		target_filename[16];
	char		unknown_2[16];
	char		image_description[64];
	char		image_version[64];
	uint32_t	mutex_bits;
	char		unknown_3[56];
	uint32_t	checksum;
} boost_hdr_t;

typedef struct image_create_args
{
	uint32_t	*kernel;
	size_t		kernel_len;
	uint32_t	*bcode;
	size_t		bcode_len;
	uint32_t	*ramdisk;
	size_t		ramdisk_len;
	uint32_t	load_offset;
	int		use_zlib;
	const char	*image_descr;
	const char	*image_version;
} image_create_args_t;

typedef struct bcode_header
{
	uint32_t	magic;
	uint32_t	bcode_off;
	uint32_t	ramdisk_size;
	uint32_t	reserved;
} bcode_hdr_t;

void boost_print_info(boost_hdr_t);
int  boost_extract(boost_hdr_t, void *);
int  boost_create(const char *, const image_create_args_t *);
int  boost_check(boost_hdr_t, const void *);

#endif /* _BOOST_H_ */
