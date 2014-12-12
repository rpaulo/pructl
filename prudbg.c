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
#include <err.h>
#include <histedit.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <libpru.h>
#include <sys/param.h>

static pru_t pru;
static unsigned int pru_number;
static uint32_t pc;

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

	snprintf(p, sizeof(p), "(pru%d) ", pru_number);

	return p;
}

#define	DECL_CMD(name)	\
	static void cmd_##name(int, const char **)

DECL_CMD(help);
DECL_CMD(quit);
DECL_CMD(run);
DECL_CMD(reset);
DECL_CMD(halt);
DECL_CMD(disassemble);

static struct commands {
	const char	*cmd;
	const char	*help;
	void		(*handler)(int, const char **);
} cmds[] = {
	{ "help", "Show a list of all commands.", cmd_help },
	{ "quit", "Quit the PRU debugger.", cmd_quit },
	{ "run", "Starts execution on the PRU.", cmd_run },
	{ "reset", "Resets the PRU.", cmd_reset },
	{ "halt", "Halts the PRU.", cmd_halt },
	{ "disassemble", "Disassemble the program.", cmd_disassemble }
};

/*
 * Implementation of all the debugger commands.
 */
static void
cmd_help(int argc __unused, const char *argv[] __unused)
{
	printf("The following is a list of built-in commands:\n\n");
	for (unsigned int i = 0; i < nitems(cmds); i++)
		printf("%-10s -- %s\n", cmds[i].cmd, cmds[i].help);
}

static void __attribute__((noreturn))
cmd_quit(int argc __unused, const char *argv[] __unused)
{
	exit(0);
}

static void
cmd_run(int argc __unused, const char *argv[] __unused)
{
	pru_enable(pru, pru_number);
	pru_wait(pru, pru_number);
}

static void
cmd_reset(int argc __unused, const char *argv[] __unused)
{
	pru_reset(pru, pru_number);
}

static void
cmd_halt(int argc __unused, const char *argv[] __unused)
{
	pru_disable(pru, pru_number);
}

static void
cmd_disassemble(int argc __unused, const char *argv[] __unused)
{
	unsigned int i;
	char buf[10];

	for (i = pc; i < 64; i += 4) {
		ti_disassemble(pru_read_mem(pru, pru_number, pc + i),
		    buf, sizeof(buf));
		printf("<0x%04x>   %s\n", pc + i, buf);
	}
}

static int
main_interface(void)
{
	EditLine *elp;
	History *hp;
	HistEvent ev;
	Tokenizer *tp;
	int count = 0;
	unsigned int i;
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
	history(hp, &ev, H_SETSIZE, 100);
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
		if (line == NULL)
			break;
		tok_str(tp, line, &t_argc, &t_argv);
		if (t_argc == 0)
			continue;
		for (i = 0; i < nitems(cmds); i++) {
			if (strcmp(t_argv[0], cmds[i].cmd) == 0) {
				cmds[i].handler(t_argc - 1, t_argv + 1);
				break;
			}
		}
		if (i == nitems(cmds)) {
			printf("error: invalid command '%s'\n", t_argv[0]);
		}
		tok_reset(tp);
	} while (count != -1);
	el_end(elp);
	history_end(hp);

	return 0;
}

int
main(int argc, char *argv[])
{
	int ch;
	char *type;
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
		warnx("missing type (-t)");
		usage();
	}
	if (argc == 0) {
	        warnx("missing binary file");
		usage();
	}
	pru_type = pru_name_to_type(type);
	if (pru_type == PRU_TYPE_UNKNOWN) {
		warnx("invalid type '%s'", type);
		usage();
	}
	pru = pru_alloc(pru_type);
	if (pru == NULL)
		err(1, "unable to allocate PRU structure");
	pru_reset(pru, pru_number);
	printf("Uploading '%s' to PRU %d: ", argv[0], pru_number);
	fflush(stdout);
	if (pru_upload(pru, pru_number, argv[0]) != 0) {
		printf("\n");
		pru_free(pru);
		err(1, "could not upload file");
	}
	printf("done.\n");

	return main_interface();
}
