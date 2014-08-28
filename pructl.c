/*-
 * Copyright (c) 2014 Rui Paulo <rpaulo@felyko.com>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libpru.h>

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stderr, "usage: %s -t type [-p pru-number] [-edr] [program]\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch;
	int reset, enable, disable, wait;
	const char *infile, *outfile;
	const char *type;
	unsigned int pru_number;
	pru_type_t pru_type;
	pru_t pru;
        int error;

	reset = enable = disable = pru_number = wait = 0;
	infile = outfile = NULL;
	type = NULL;
	error = 0;
	while ((ch = getopt(argc, argv, "t:p:edrw")) != -1) {
		switch (ch) {
		case 't':
			type = optarg;
			break;
		case 'p':
			pru_number = (unsigned int)atoi(optarg);
			break;
		case 'e':
			enable = 1;
			break;
		case 'd':
			disable = 1;
			break;
		case 'r':
			reset = 1;
			break;
		case 'w':
			wait = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (type == NULL) {
		fprintf(stderr, "%s: missing type (-t)\n", getprogname());
		usage();
	}
	pru_type = pru_name_to_type(type);
	if (pru_type == PRU_TYPE_UNKNOWN) {
		fprintf(stderr, "%s: invalid type '%s'\n", getprogname(),
		    type);
		return 2;
	}
	if (infile && !outfile) {
		fprintf(stderr, "%s: missing output file (-o)\n",
		    getprogname());
		usage();
	}
	pru = pru_alloc(pru_type);
	if (pru == NULL) {
		fprintf(stderr, "%s: unable to allocate pru structure\n",
		    getprogname());
		return 3;
	}
	if (reset) {
		error = pru_reset(pru, pru_number);
		if (error) {
			fprintf(stderr, "%s: unable to reset PRU %d\n",
			    getprogname(), pru_number);
			return 6;
		}
	}
	if (argc > 1) {
		error = pru_upload(pru, pru_number, argv[0]);
		if (error) {
			fprintf(stderr, "%s: unable to upload %s\n",
			    getprogname(), argv[0]);
			return 7;
		}
	}
	if (enable) {
		error = pru_enable(pru, pru_number);
		if (error) {
			fprintf(stderr, "%s: unable to enable PRU %d\n",
			    getprogname(), pru_number);
			return 4;
		}
	}
	if (disable) {
		error = pru_disable(pru, pru_number);
		if (error) {
			fprintf(stderr, "%s: unable to disable PRU %d\n",
			    getprogname(), pru_number);
			return 5;
		}
	}
	if (wait) {
		error = pru_wait(pru, pru_number);
		if (error) {
			fprintf(stderr, "%s: unable to wait for PRU %d\n",
			    getprogname(), pru_number);
			return 8;
		}
	}
}
