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
#include <libutil.h>
#include <sys/param.h>

static pru_t pru;
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

	snprintf(p, sizeof(p), "(pru%d) ", pru_number);

	return p;
}

#define	DECL_CMD(name)	\
	static void cmd_##name(int, const char **)

DECL_CMD(breakpoint);
DECL_CMD(disassemble);
DECL_CMD(halt);
DECL_CMD(help);
DECL_CMD(memory);
DECL_CMD(quit);
DECL_CMD(reset);
DECL_CMD(register);
DECL_CMD(run);

static struct commands {
	const char	*cmd;
	const char	*help;
	void		(*handler)(int, const char **);
} cmds[] = {
	{ "breakpoint", "Manage breakpoints.", cmd_breakpoint },
	{ "disassemble", "Disassemble the program.", cmd_disassemble },
	{ "halt", "Halts the PRU.", cmd_halt },
	{ "help", "Show a list of all commands.", cmd_help },
	{ "memory", "Inspect PRU memory.", cmd_memory },
	{ "quit", "Quit the PRU debugger.", cmd_quit },
	{ "reset", "Resets the PRU.", cmd_reset },
	{ "register", "Operates on registers.", cmd_register },
	{ "run", "Starts the PRU.", cmd_run },
};

/*
 * Implementation of all the debugger commands.
 */
static void
cmd_help(int argc __unused, const char *argv[] __unused)
{
	printf("The following is a list of built-in commands:\n\n");
	for (unsigned int i = 0; i < nitems(cmds); i++)
		printf("%-11s -- %s\n", cmds[i].cmd, cmds[i].help);
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
cmd_disassemble(int argc, const char *argv[])
{
	unsigned int i, start, end;
	char buf[32];
	uint32_t pc;

	pc = pru_read_reg(pru, pru_number, REG_PC);
	if (argc > 0)
		start = (uint32_t)strtoul(argv[0], NULL, 10);
	else
		start = pc;
	if (argc > 1)
		end = start + (uint32_t)strtoul(argv[1], NULL, 10);
	else
		end = start + 16;
	for (i = start; i < end; i += 4) {
		pru_disassemble(pru, pru_read_imem(pru, pru_number, i),
		    buf, sizeof(buf));
		if (pc == i)
			printf("-> ");
		else
			printf("   ");
		printf("0x%04x:  %s\n", i, buf);
	}
}

static void
cmd_breakpoint(int argc, const char *argv[] __unused)
{
	if (argc == 0) {
		printf("The following sub-commands are supported:\n\n");
		printf("delete -- Deletes a breakpoint (or all).\n");
		printf("list   -- Lists all breakpoints.\n");
		printf("set    -- Creates a breakpoint.\n");
	}
}

static enum pru_reg
reg_name_to_enum(const char *name)
{
	unsigned int reg;

	if (strcmp(name, "pc") == 0)
		return REG_PC;
	else if (name[0] == 'r')
		name++;
	reg = (unsigned int)strtoul(name, NULL, 10);
	if (reg >= REG_R0 && reg <= REG_R31)
		return reg;
	else
		return REG_INVALID;
}

static void
cmd_register(int argc, const char *argv[])
{
	unsigned int i;
	uint32_t val;

	if (argc == 0) {
		printf("The following sub-commands are supported:\n\n");
		printf("read  -- Reads a register or all.\n");
		printf("write -- Modifies a register.\n");
		return;
	}
	if (strcmp(argv[0], "read") == 0) {
		if (argc > 1) {
			if (strcmp(argv[1], "all") == 0) {
				for (i = REG_R0; i <= REG_R31; i++)
					printf("  r%u = 0x%x\n", i,
					    pru_read_reg(pru, pru_number, i));
				printf("  pc = 0x%x\n",
				    pru_read_reg(pru, pru_number, REG_PC));
			} else {
				printf("  %s = 0x%x\n", argv[1],
				    pru_read_reg(pru, pru_number,
					reg_name_to_enum(argv[1])));
			}
		} else
			printf("error: missing register name\n");
	} else if (strcmp(argv[0], "write") == 0) {
		if (argc > 2) {
			val = (uint32_t)strtoul(argv[2], NULL, 10);
			pru_write_reg(pru, pru_number,
			    reg_name_to_enum(argv[1]), val);
		} else
			printf("error: missing register and/or value\n");
	} else
		printf("error: unsupported sub-command\n");
}

static void
cmd_memory(int argc, const char *argv[])
{
	uint8_t *buf;
	size_t size, i, addr;

	if (argc == 0) {
		printf("The following sub-commands are supported:\n\n");
		printf("read  -- Read from the PRU memory.\n");
		printf("write -- Write to the PRU memory.\n");
		return;
	}
	if (strcmp(argv[0], "read") == 0) {
		if (argc > 1)
			addr = strtoul(argv[1], NULL, 10);
		else
			addr = 0;
		if (argc > 2)
			size = strtoul(argv[2], NULL, 10);
		else
			size = 128;
		buf = malloc(size);
		for (i = 0; i < size; i++) {
			buf[i] = pru_read_mem(pru, pru_number, 0);
		}
		hexdump(buf, (int)size, NULL, 0);
		free(buf);
	} else if (strcmp(argv[0], "write") == 0) {
		/* TODO */
	} else
		printf("error: unsupported sub-command\n");
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
	el_set(elp, EL_EDITOR, "emacs");
	el_source(elp, NULL);
	do {
		line = el_gets(elp, &count);
		if (line == NULL)
			break;
		tok_str(tp, line, &t_argc, &t_argv);
		if (t_argc == 0)
			continue;
		history(hp, &ev, H_ENTER, line);
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
			pru_number = (unsigned int)strtoul(optarg, NULL, 10);
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
