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

#define OFFSET_2_BRANCHL(off) ((0xeb << 24) | \
	(((off - 8) >> 2) & 0x00ffffff))
#define BRANCHL_2_OFFSET(br) (((br & 0x00ffffff) << 2) + 8)

#define OFFSET_2_BRANCH(off) ((0xea << 24) | \
	(((off + 248) >> 2) & 0x00ffffff))
#define BRANCH_2_OFFSET(br) (((br & 0x00ffffff) << 2) - 248)

/* Forward declarations of local functions */
int  boost_create_adv(const char *, const image_create_args_t *);
int  boost_create_simple(const char *, const image_create_args_t *);
int  boost_extract_new(const uint32_t *, size_t);
int  boost_extract_legacy(const uint32_t *, size_t);
void boost_setup_header(boost_hdr_t *, const uint32_t *, size_t, const image_create_args_t *);
int  boost_is_legacy(const boost_hdr_t *hdr);
int  bcode_check(uint32_t);

void
boost_print_info(boost_hdr_t hdr)
{
	char platid_str[5];

	memset(platid_str, 0, 5);
	memcpy(platid_str, &hdr.platform_id, 4);

	printf("  Type:            ");
	if (hdr.image_id == BOOST_EXEC_ID) {
		printf("Bootable image\n");
	} else if (hdr.image_id == BOOST_SCRIPT_ID) {
		printf("BooSt Script\n");
	} else if (hdr.image_id == BOOST_BOOT_ID) {
		printf("BooSt Operating System\n");
	} else if (hdr.image_id == BOOST_PCON_ID) {
		printf("Pcon firmware\n");
	} else {
		printf("Unknown (0x%x)\n", hdr.image_id);
	}


	printf("  Platform:        %s\n", platid_str);
	printf("  Description:     %s\n", hdr.image_description);
	printf("  Version:         %s\n", hdr.image_version);

	if (hdr.target_filename[0]) {
		printf("  Target filename: %s\n", hdr.target_filename);
	}

	if (hdr.image_id == BOOST_EXEC_ID) {
		printf("  Load offset:     0x%08x\n", hdr.load_offset);
		printf("  Branch offset:   %u\n", BRANCH_2_OFFSET(hdr.branch_offset));
		/*
		printf("  Mutex bits:      0x%x\n", hdr.mutex_bits);
		*/
	}

	printf("  Size:            %u\n", hdr.image_size);
	printf("  Header CRC:      %u\n", hdr.checksum);
	printf("  Image CRC:       %u\n", hdr.image_checksum);

	if (hdr.flags) {
		printf("  Flags:           0x%08x\n", hdr.flags);
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
}

int
boost_extract(boost_hdr_t hdr, void *data)
{
	uint32_t first_instr = 0;
	size_t len = 0;
	int rv = 0;

	if (0 != boost_check(hdr, data)) {
		return 1;
	}

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
		if (boost_is_legacy(&hdr)) {
			rv = boost_extract_legacy(data, len);
		} else {
			rv = boost_extract_new(data, len);
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

int boost_create(const char *outfile, const image_create_args_t *cargs)
{
	if (cargs->kernel && cargs->bcode) {
		return boost_create_adv(outfile, cargs);
	} else if (cargs->kernel && !cargs->bcode && !cargs->ramdisk) {
		return boost_create_simple(outfile, cargs);
	} else {
		fprintf(stderr, "Unsupported input arguments combination!\n");
		return 1;
	}
}

int boost_create_adv(const char *outfile, const image_create_args_t *cargs)
{
	uint32_t *image_buf = NULL, *copy_ptr = NULL, *image_data = NULL;
	boost_hdr_t *boost_hdr = NULL;
	bcode_hdr_t *bcode_hdr = NULL;
	void *zlib_data = NULL;
	size_t zlib_data_len;
	size_t buf_len, payload_len;
	uint32_t branch_offset;
	int rv = 1;

	if (0 == bcode_check(cargs->bcode[0])) {
		return 1;
	}

	buf_len = cargs->kernel_len + cargs->bcode_len + cargs->ramdisk_len;
	buf_len += STARTUP_BYTES; /* Initial branch instruction */
	payload_len = buf_len;

	image_buf = malloc(buf_len);
	if (NULL == image_buf) {
		fprintf(stderr, "Failed to allocate output buffer!\n");
		return 1;
	}

	branch_offset = STARTUP_BYTES + cargs->kernel_len + sizeof(bcode_hdr_t);
	image_buf[0] = OFFSET_2_BRANCHL(branch_offset);

	copy_ptr = image_buf + STARTUP_BYTES / sizeof(uint32_t);
	memcpy(copy_ptr, cargs->kernel, cargs->kernel_len);

	copy_ptr += (cargs->kernel_len / sizeof(uint32_t));
	bcode_hdr = (bcode_hdr_t *)(copy_ptr);
	memcpy(copy_ptr, cargs->bcode, cargs->bcode_len);

	copy_ptr += (cargs->bcode_len / sizeof(uint32_t));
	if (NULL != cargs->ramdisk)
		memcpy(copy_ptr, cargs->ramdisk, cargs->ramdisk_len);

	/* Set bootcode configuration fields. */
	bcode_hdr->bcode_off = branch_offset - sizeof(bcode_hdr_t);
	bcode_hdr->ramdisk_size = cargs->ramdisk_len;
#if 0
	if (0 != write_to_file((char *)image_buf, buf_len, "payload.bin")) {
		fprintf(stderr, "Failed to write payload.bin!\n");
		goto create_failed;
	}
#endif
	zlib_data = zlib_compress((char *)image_buf, buf_len, &zlib_data_len);
	if (NULL == zlib_data) {
		fprintf(stderr, "Failed to compress image!\n");
		goto create_failed;
	}

	free(image_buf);
	image_buf = NULL;
	copy_ptr = NULL;
	bcode_hdr = NULL;
#if 0
	if (0 != write_to_file((char *)zlib_data, zlib_data_len, "payload.zlib")) {
		fprintf(stderr, "Failed to write payload.zlib!\n");
		goto create_failed;
	}
#endif
	buf_len = zlib_data_len + sizeof(boost_hdr_t) + sizeof(uint32_t);
	image_buf = malloc(buf_len);
	if (NULL == image_buf) {
		fprintf(stderr, "Failed to allocate output buffer!\n");
		goto create_failed;
	}

	boost_hdr = (boost_hdr_t *)image_buf;
	image_data = image_buf + sizeof(boost_hdr_t) / sizeof(uint32_t);
	image_data[0] = swap_bytes_be(payload_len);
	memcpy(image_data + 1, zlib_data, zlib_data_len);

	free(zlib_data);
	zlib_data = NULL;

	boost_setup_header(boost_hdr, image_data, buf_len - sizeof(boost_hdr_t), cargs);

	if (0 != write_to_file((char *)image_buf, buf_len, outfile)) {
		fprintf(stderr, "Failed to write %s\n", outfile);
		goto create_failed;
	}

	rv = 0;

create_failed:
	if (NULL != image_buf) {
		free(image_buf);
	}
	if (NULL != zlib_data) {
		free(zlib_data);
	}

	return rv;
}

int
boost_create_simple(const char *outfile, const image_create_args_t *cargs)
{
	uint32_t *data = NULL, *image_buf = NULL, *copy_dest = NULL;
	size_t data_len = 0, image_buf_len = 0;
	boost_hdr_t *hdr = NULL;
	int rv = 1;

	if (cargs->use_zlib) {
		data = zlib_compress((char *)cargs->kernel, cargs->kernel_len, &data_len);
		if (NULL == data) {
			fprintf(stderr, "Failed to compress image!\n");
			goto create_simple_failed;
		}
		data_len += 4; /* Unpacked data size field. */
	} else {
		data = cargs->kernel;
		data_len = cargs->kernel_len;
	}

	image_buf_len = sizeof(boost_hdr_t) + data_len;
	image_buf = malloc(image_buf_len);
	if (NULL == image_buf) {
		fprintf(stderr, "Failed to allocate image buffer\n");
		goto create_simple_failed;
	}

	hdr = (boost_hdr_t *)image_buf;
	copy_dest = (image_buf + sizeof(boost_hdr_t) / 4);

	if (cargs->use_zlib) {
		memcpy(copy_dest + 1, data, data_len - 4);
		copy_dest[0] = swap_bytes_be(cargs->kernel_len);
	} else {
		memcpy(copy_dest, data, data_len);
	}

	boost_setup_header(hdr, copy_dest, data_len, cargs);

	if (0 != write_to_file((char *)image_buf, image_buf_len, outfile)) {
		fprintf(stderr, "Failed to write %s\n", outfile);
		goto create_simple_failed;
	}

	rv = 0;

create_simple_failed:
	if (data && cargs->use_zlib) {
		free(data);
		data = NULL;
	}
	if (image_buf) {
		free(image_buf);
		image_buf = NULL;
	}

	return rv;
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
boost_extract_new(const uint32_t *data, size_t len)
{
	uint32_t kern_off, bcode_off, rdisk_off;
	size_t kern_size, bcode_size, rdisk_size;
	bcode_hdr_t *hdr;

	kern_off = STARTUP_BYTES;
	bcode_off = BRANCHL_2_OFFSET(data[0]) - sizeof(bcode_hdr_t);
	kern_size = bcode_off - kern_off;

	hdr = (bcode_hdr_t *)(data + bcode_off / sizeof(uint32_t));

	bcode_size = (len - hdr->ramdisk_size) - bcode_off;
	rdisk_off = len - hdr->ramdisk_size;
	rdisk_size = hdr->ramdisk_size;

	printf("Kernel image\t: offset = 0x%08x, size = %zdkB\n",
	        kern_off, kern_size/1024);
	if (0 != write_to_file(((char *)data) + kern_off, kern_size, "Image")) {
		printf("Writing uImage\t: Failed\n");
		return 1;
	}
	printf("Writing uImage\t: OK\n");

	printf("Bootstrap image\t: offset = 0x%08x, size = %zdB\n",
	       bcode_off, bcode_size);
	if (0 != write_to_file(((char *)data) + bcode_off, bcode_size, "bcode")) {
		printf("Writing bcode\t: Failed\n");
		return 1;
	}
	printf("Writing bcode\t: OK\n");

	printf("RAM disk image\t: offset = 0x%08x, size = %zdkB\n",
	       rdisk_off, rdisk_size/1024);
	if (0 != write_to_file(((char *)data) + rdisk_off, rdisk_size, "initrd.ext2")) {
		printf("Writing initrd\t: Failed\n");
		return 1;
	}
	printf("Writing initrd\t: OK\n");

	return 0;
}

int
boost_extract_legacy(const uint32_t *data, size_t len)
{
	uint32_t kern_off, bcode_off, rdisk_off;
	size_t kern_size, bcode_size, rdisk_size;

	kern_off = 4;
	bcode_off = BRANCHL_2_OFFSET(data[0]) - LEGACY_BCODE_START_OFFSET;
	kern_size = bcode_off - kern_off;
	bcode_size = LEGACY_BCODE_SIZE;
	rdisk_off = bcode_off + bcode_size;
	rdisk_size = len - rdisk_off;

	printf("Kernel image\t: offset = 0x%08x, size = %zdkB\n",
	        kern_off, kern_size/1024);
	if (0 != write_to_file(((char *)data) + kern_off, kern_size, "Image")) {
		printf("Writing uImage\t: Failed\n");
		return 1;
	}
	printf("Writing uImage\t: OK\n");

	printf("Bootstrap image\t: offset = 0x%08x, size = %zdB\n",
	       bcode_off, bcode_size);
	if (0 != write_to_file(((char *)data) + bcode_off, bcode_size, "bcode-legacy")) {
		printf("Writing bcode\t: Failed\n");
		return 1;
	}
	printf("Writing bcode\t: OK\n");

	printf("RAM disk image\t: offset = 0x%08x, size = %zdkB\n",
	       rdisk_off, rdisk_size/1024);
	if (0 != write_to_file(((char *)data) + rdisk_off, rdisk_size, "initrd.ext2")) {
		printf("Writing initrd\t: Failed\n");
		return 1;
	}
	printf("Writing initrd\t: OK\n");

	return 0;
}

void boost_setup_header(boost_hdr_t *hdr, const uint32_t *data, size_t data_len,
                        const  image_create_args_t *ic)
{
	memset(hdr, 0, sizeof(boost_hdr_t));

	hdr->branch_offset = OFFSET_2_BRANCH(0);
	hdr->image_size = data_len;
	hdr->image_checksum = cksum((char *)data, data_len);
	hdr->load_offset = ic->load_offset;

	hdr->flags |= BOOST_FLAG_RAM_IMG;
	hdr->flags |= BOOST_FLAG_NO_HDR;

	if (ic->use_zlib)
		hdr->flags |= BOOST_FLAG_ZLIB;

	memcpy(&(hdr->platform_id), "nBk2", 4);
	memcpy(hdr->target_filename, "nBkProOs.img", 12);
	strncpy(hdr->image_description, ic->image_descr, 64);
	strncpy(hdr->image_version, ic->image_version, 64);

	hdr->checksum = cksum((const char *)hdr, BOOST_HEADER_CRC_BYTES);
}

int boost_is_legacy(const boost_hdr_t *hdr)
{
	if (0 == strncmp(hdr->image_version, "K123m", 5)) {
		return 1;
	} else {
		return 0;
	}
}

int bcode_check(uint32_t arg)
{
	int magic = (arg & BCODE_MAGIC_MASK);
	int version = (arg & BCODE_VERSION_MASK);

	if (magic != BCODE_MAGIC) {
		printf("Invalid bootcode image!\n");
		return 0;
	}

	if (version != 1) {
		printf("Unsupported bootcode version = %d!\n", version);
		return 0;
	}

	return 1;
}
