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
#include <limits.h>
#include <libgen.h>
#include <errno.h>
#include <stdio.h>

#include "cmd.h"
#include "config.h"


void
print_help(char *progname)
{
	printf("Psion/Teklogix NetBook Pro BooSt image tool, version %s\n"
	       "Usage: %s [command] [command options]\n\n"
	       "Command syntax:\n"
	       "  check filename\n"
               "  create [create args]\n"
	       "  extract filename\n"
	       "  info filename\n\n"
	       "Possible create paramaters:\n"
	       "  -k kernel, path to kernel image\n"
	       "  -b bootcode, path to boot code binary\n"
	       "  -r ramdisk, path to ramdisk image\n"
	       "  -o outfile, name of the output image\n"
	       "  -d descr, image description\n"
	       "  -v version, image version string\n"
	       "  -l offset, memory load offset\n"
	       "  -z, use zlib compression\n",
	       VERSION_STR, basename(progname));
}

int
parse_create_args(int argc, char *argv[], create_args_t *args)
{
	int i;

	for (i = 2; i < argc; i++) {
		if ((0 == strncmp(argv[i], "-k", 2)) && (++i < argc)) {
			args->kernel = argv[i];
		} else if ((0 == strncmp(argv[i], "-b", 2)) && (++i < argc)) {
			args->bcode = argv[i];
		} else if ((0 == strncmp(argv[i], "-r", 2)) && (++i < argc)) {
			args->ramdisk = argv[i];
		} else if ((0 == strncmp(argv[i], "-o", 2)) && (++i < argc)) {
			args->outfile = argv[i];
		} else if ((0 == strncmp(argv[i], "-d", 2)) && (++i < argc)) {
			args->image_descr = argv[i];
		} else if ((0 == strncmp(argv[i], "-v", 2)) && (++i < argc)) {
			args->image_version = argv[i];
		} else if ((0 == strncmp(argv[i], "-l", 2)) && (++i < argc)) {
			errno = 0;
			args->load_offset = strtol(argv[i], (char **)NULL, 16);
			if (errno != 0) {
				printf("Invalid load offset specified!\n");
				return 1;
			}
		} else if (0 == strncmp(argv[i], "-z", 2)) {
			args->use_zlib = 1;
		} else {
			printf("Invalid create arguments!\n");
			return 1;
		}
	}

	if (args->kernel == NULL) {
		printf("Kernel path has to be specified!\n");
		return 1;
	}
	if (args->outfile == NULL) {
		args->outfile = DEFAULT_BOOST_IMG_NAME;
	}
	if (args->image_descr == NULL) {
		args->image_descr = DEFAULT_BOOST_IMG_DESCR;
	}
	if (args->image_version == NULL) {
		args->image_version = DEFAULT_BOOST_IMG_VER;
	}
	if (args->load_offset == 0) {
		args->load_offset = DEFAULT_IMG_LOAD_OFFSET;
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	create_args_t create_args;

	if (argc < 3) {
		print_help(argv[0]);
		return 1;
	}

	if (0 == strncmp(argv[1], "info", 4)) {
		return cmd_info(argv[2]);
	} else if (0 == strncmp(argv[1], "extract", 5)) {
		return cmd_extract(argv[2]);
	} else if (0 == strncmp(argv[1], "check", 5)) {
		return cmd_check(argv[2]);
	} else if (0 == strncmp(argv[1], "create", 6)) {
		memset(&create_args, 0, sizeof(create_args_t));
		if (parse_create_args(argc, argv, &create_args)) {
			print_help(argv[0]);
			return 1;
		}
		return cmd_create(&create_args);
	} else {
		print_help(argv[0]);
		return 1;
	}

	return 0;
}
