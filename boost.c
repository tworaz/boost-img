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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "boost.h"
#include "util.h"
#include "config.h"

#define IS_ARM_BRANCH(ins) (((ins) & (0xea << 24)) | \
                            ((ins) & (0xeb << 24)))

#define BRANCH_2_OFFSET(br) (((br & 0x00ffffff) << 2) + 8)
#define OFFSET_2_BRANCH(off) ((0xea << 24) | \
	(((off - 8) >> 2) & 0x00ffffff))

/* Forward declarations of local functions */
int boost_split(boost_hdr_t hdr, const uint32_t *img, size_t len, uint32_t tag);

void
boost_print_info(boost_hdr_t hdr)
{
	char platid_str[5];

	memset(platid_str, 0, 5);
	memcpy(platid_str, &hdr.platform_id, 4);

	printf("  Platform ID:     %s\n", platid_str);
	printf("  Target filename: %s\n", hdr.target_filename);
	printf("  Load offset:     0x%08x\n", hdr.load_offset);
	printf("  Branch offset:   %u\n", BRANCH_2_OFFSET(hdr.branch_offset));
	printf("  Mutex bits:      0x%x\n", hdr.mutex_bits);
	printf("  Header checksum: %u\n", hdr.checksum);
	printf("  Image:\n");
	printf("    * ID:          %u\n", hdr.image_id);
	printf("    * Name:        %s\n", hdr.image_name);
	printf("    * Version:     %s\n", hdr.image_version_string);
	printf("    * Size:        %u\n", hdr.image_size);
	printf("    * Checksum:    %u\n", hdr.image_checksum);

	printf("  Flags (0x%08x):\n", hdr.flags);
	if (hdr.flags & BOOST_FLAG_RAM_IMG)
		printf("    * RAM image\n");
	if (hdr.flags & BOOST_FLAG_NO_HDR)
		printf("    * No header\n");
	if (hdr.flags & BOOST_FLAG_ERASE)
		printf("    * Erase before saving\n");
	if (hdr.flags & BOOST_FLAG_NO_RST)
		printf("    * No reset before starting\n");
	if (hdr.flags & BOOST_FLAG_COPY_OS)
		printf("    * Copy to OS partition\n");
	if (hdr.flags & BOOST_FLAG_ZLIB)
		printf("    * Compressed with zlib\n");
	if (hdr.flags & BOOST_FLAG_SPLASH)
		printf("    * Has splash\n");
}

int
boost_extract(boost_hdr_t hdr, void *data)
{
	uint32_t first_instr = 0;
	uint32_t tag;
	size_t len = 0;
	int rv = 0;

	if (0 != boost_check(hdr, data)) {
		return 1;
	}

	tag = ((uint32_t *)data)[0];

	if (hdr.flags & BOOST_FLAG_ZLIB) {
		// Actual zlib stream begins 4 bytes into the data section
		data = zlib_decompress(data + 4, hdr.image_size, &len);
		if (data == NULL) {
			return 1;
		}
		printf("Zlib unpack\t: OK\n");
	} else {
		len = hdr.image_size;
	}

	first_instr = ((uint32_t *)(data))[0];
	if (IS_ARM_BRANCH(first_instr)) {
		if (0 != boost_split(hdr, data, len, tag)) {
			rv = 1;
		}
	} else {
		printf("Warning: unknown image format!\n");
		if (0 != write_to_file(data, len, "payload.bin")) {
			printf("Writing payload.bin\t: Failed\n");
			rv = 1;
			goto extract_cleanup;
		}
		printf("Writing payload.bin\t: OK\n");
	}

extract_cleanup:
	if (hdr.flags & BOOST_FLAG_ZLIB) {
		free(data);
	}

	return rv;
}

int
boost_create(const char *outfile, const image_components_t *parts)
{
	printf("TODO: implement %s\n", __func__);
	return 1;
}

int
boost_check(boost_hdr_t hdr, const void *data)
{
	uint32_t data_crc = 0;
	uint32_t hdr_crc = 0;
	int rv = 0;

	data_crc = cksum(data, hdr.image_size);
	hdr_crc = cksum((const char *)&hdr, BOOST_HEADER_CRC_BYTES);

	if (hdr_crc == hdr.checksum) {
		printf("Header checksum\t: OK\n");
	} else {
		printf("Header checksum\t: Failed (expected %u, got %u)\n",
		       hdr.checksum, hdr_crc);
		rv = 1;
	}

	if (data_crc == hdr.image_checksum) {
		printf("Image checksum\t: OK\n");
	} else {
		printf("Image checksu\t: Failed (expected %u, got %u)\n",
		       hdr.image_checksum, data_crc);
		rv = 1;
	}

	return rv;
}

int
boost_split(boost_hdr_t hdr, const uint32_t *img, size_t len, uint32_t tag)
{
	uint32_t kern_off, bcode_off, rdisk_off;
	size_t kern_size, bcode_size, rdisk_size;

	kern_off = 4;
	bcode_off = BRANCH_2_OFFSET(img[0]) - 408;
	kern_size = bcode_off - kern_off;
	bcode_size = 1052;
	rdisk_off = bcode_off + bcode_size;
	rdisk_size = len - rdisk_off;

	printf("Kernel image\t: offset = 0x%08x, size = %zdkB\n",
	        kern_off, kern_size/1024);
	if (0 != write_to_file(((char *)img) + kern_off, kern_size, "uImage")) {
		printf("Writing uImage\t: Failed\n");
		return 1;
	}
	printf("Writing uImage\t: OK\n");

	printf("Bootstrap image\t: offset = 0x%08x, size = %zdB\n",
	       bcode_off, bcode_size);
	if (0 != write_to_file(((char *)img) + bcode_off, bcode_size, "bcode")) {
		printf("Writing bcode\t: Failed\n");
		return 1;
	}
	printf("Writing bcode\t: OK\n");

	printf("RAM disk image\t: offset = 0x%08x, size = %zdkB\n",
	       rdisk_off, rdisk_size/1024);
	if (0 != write_to_file(((char *)img) + rdisk_off, rdisk_size, "initrd")) {
		printf("Writing initrd\t: Failed\n");
		return 1;
	}
	printf("Writing initrd\t: OK\n");

	return 0;
}
