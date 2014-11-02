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
#include <unistd.h>
#include <histedit.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <libpru.h>

static unsigned int pru_number;

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stderr, "usage: %s -t type [-p pru-number] <program>\n",
	    getprogname());
	exit(1);
}

static const char *
prompt(void)
{
	static char p[8];

	snprintf(p, sizeof(p), "pru%d> ", pru_number);

	return p;
}

static int
main_interface(pru_t pru __unused)
{
	EditLine *elp;
	History *hp;
	Tokenizer *tp;
	int count = 0;
	const char *line;
	int t_argc;
	const char **t_argv;

	elp = el_init(getprogname(), stdin, stdout, stderr);
	if (elp == NULL)
		return 1;
	hp = history_init();
	if (hp == NULL) {
		el_end(elp);
		return 1;
	}
	tp = tok_init(NULL);
	if (tp == NULL) {
		history_end(hp);
		el_end(elp);
		return 1;
	}
	el_set(elp, EL_PROMPT, prompt);
	el_set(elp, EL_HIST, history, hp);
	el_set(elp, EL_SIGNAL, 1);
	el_source(elp, NULL);
	do {
		line = el_gets(elp, &count);
		if (line) {
			tok_str(tp, line, &t_argc, &t_argv);
			if (t_argc) {
				printf("%s\n", t_argv[0]);
			}
		}
		tok_reset(tp);
	} while (line != NULL && count != -1);
	el_end(elp);
	history_end(hp);

	return 0;
}

int
main(int argc, char *argv[])
{
	int ch;
	char *type;
	pru_t pru;
	pru_type_t pru_type;

	type = NULL;
	while ((ch = getopt(argc, argv, "p:t:")) != -1) {
		switch (ch) {
		case 'p':
			pru_number = (unsigned int)atoi(optarg);
			break;
		case 't':
			type = optarg;
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (type == NULL) {
		fprintf(stderr, "%s: missing type (-t)\n", getprogname());
		usage();
	}
	if (argc == 0) {
		fprintf(stderr, "%s: missing binary file\n", getprogname());
		usage();
	}
	pru_type = pru_name_to_type(type);
	if (pru_type == PRU_TYPE_UNKNOWN) {
		fprintf(stderr, "%s: invalid type '%s'\n", getprogname(),
		    type);
		usage();
	}
	pru = pru_alloc(pru_type);
	if (pru == NULL) {
		fprintf(stderr, "%s: unable to allocate PRU structure: %s\n",
		    getprogname(), strerror(errno));
		return 1;
	}
	pru_reset(pru, pru_number);

	return main_interface(pru);
}
