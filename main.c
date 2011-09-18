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

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "cmd.h"
#include "config.h"


void
print_help(const char *progname)
{
	printf("Psion/Teklogix NetBook Pro BooSt image tool, version %s\n"
	       "Usage: %s [command] [command options]\n\n"
	       "Command syntax:\n"
	       "  check filename\n"
	       "  create [-z] kernel bootcode ramdisk filename\n"
	       "  extract filename\n"
	       "  info filename\n\n"
	       "Command parameters:\n"
	       "  'filename' netbook pro boost image file\n"
	       "  'kernel' linux kernel image\n"
	       "  'ramdisk' linux ramdisk image\n"
	       "  'bootstrap' low level initialization code\n"
	       "  '-z' use zlib compression\n",
	       VERSION_STR, progname);
}

int
main(int argc, char *argv[])
{
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
		if (0 == strncmp(argv[2], "-z", 2)) {
			if (argc < 7) {
				print_help(argv[0]);
				return 1;
			}
			return cmd_create(argv[3], argv[4], argv[5],
			                  argv[6], true);
		} else {
			if (argc < 6) {
				print_help(argv[0]);
				return 1;
			}
			return cmd_create(argv[2], argv[3], argv[4],
			                  argv[5], false);
		}
	} else {
		print_help(argv[0]);
		return 1;
	}

	return 0;
}
