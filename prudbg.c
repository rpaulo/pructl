/*-
 * Copyright (c) 2014-2015 Rui Paulo <rpaulo@felyko.com>
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
#include <sys/queue.h>
#include <sys/param.h>

static pru_t pru;
static unsigned int pru_number;
static unsigned int last_breakpoint;

struct breakpoint {
	uint32_t n;
	uint32_t addr;
	uint32_t orig_instr;
	uint32_t resv;
	LIST_ENTRY(breakpoint) entry;
};

static struct breakpoint *in_breakpoint;

static LIST_HEAD(, breakpoint) breakpoint_list =
	LIST_HEAD_INITIALIZER(breakpoint_list);

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stderr, "usage: %s -t type [-p pru-number] <program>\n",
	    getprogname());
	fprintf(stderr, "usage: %s -t type [-p pru-number] -a\n",
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
DECL_CMD(continue);
DECL_CMD(disassemble);
DECL_CMD(halt);
DECL_CMD(help);
DECL_CMD(memory);
DECL_CMD(quit);
DECL_CMD(reset);
DECL_CMD(register);
DECL_CMD(run);
DECL_CMD(step);

static struct command {
	const char	*cmd;
	const char	*help;
	void		(*handler)(int, const char **);
} cmds[] = {
	{ "breakpoint", "Manage breakpoints.", cmd_breakpoint },
	{ "continue", "Continue after a breakpoint.", cmd_continue },
	{ "disassemble", "Disassemble the program.", cmd_disassemble },
	{ "halt", "Halt the PRU.", cmd_halt },
	{ "help", "Show a list of all commands.", cmd_help },
	{ "memory", "Inspect PRU memory.", cmd_memory },
	{ "quit", "Quit the PRU debugger.", cmd_quit },
	{ "reset", "Reset the PRU.", cmd_reset },
	{ "register", "Operate on registers.", cmd_register },
	{ "run", "Start the PRU.", cmd_run },
	{ "step", "Single step an instruction.", cmd_step },
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
	struct breakpoint *bp;
	uint32_t pc;

	pru_enable(pru, pru_number, 0);
	pru_wait(pru, pru_number);
	pc = pru_read_reg(pru, pru_number, REG_PC);
	LIST_FOREACH(bp, &breakpoint_list, entry) {
		if (bp->addr == pc)
			break;
	}
	in_breakpoint = bp;
	if (bp == NULL)
		printf("PRU%d halted normally\n", pru_number);
	else {
		printf("PRU%d halted, breakpoint %d, address 0x%x\n",
		    pru_number, bp->n, pc);
		cmd_disassemble(0, NULL);
	}
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
	uint32_t pc, ins;
	struct breakpoint *bp;

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
		LIST_FOREACH(bp, &breakpoint_list, entry) {
			if (bp->addr == i)
				break;
		}
		if (bp != NULL)
			ins = bp->orig_instr;
		else
			ins = pru_read_imem(pru, pru_number, i);
		pru_disassemble(pru, ins, buf, sizeof(buf));
		if (pc == i)
			printf("-> ");
		else
			printf("   ");
		printf("0x%04x:  %s", i, buf);
		if (bp)
			printf("\t; breakpoint %d\n", bp->n);
		else
			puts("");
	}
}

static void
cmd_breakpoint(int argc, const char *argv[])
{
	uint32_t addr;
	struct breakpoint *bp;
	uint32_t bpn;

	if (argc == 0) {
		printf("The following sub-commands are supported:\n\n");
		printf("delete -- Deletes a breakpoint (or all).\n");
		printf("list   -- Lists all breakpoints.\n");
		printf("set    -- Creates a breakpoint.\n");
		return;
	}
	if (strcmp(argv[0], "delete") == 0) {
		if (argc < 2) {
			printf("error: missing breakpoint number\n");
			return;
		}
		bpn = (uint32_t)strtoul(argv[1], NULL, 10);
		LIST_FOREACH(bp, &breakpoint_list, entry) {
			if (bp->n == bpn)
				break;
		}
		if (bp == NULL) {
			printf("error: breakpoint not found\n");
			return;
		}
		/* TODO: restore instruction */
		LIST_REMOVE(bp, entry);
		free(bp);
	} else if (strcmp(argv[0], "list") == 0) {
		LIST_FOREACH(bp, &breakpoint_list, entry) {
			printf("Breakpoint %d at 0x%x\n", bp->n,
			    bp->addr);
		}
	} else if (strcmp(argv[0], "set") == 0) {
		if (argc < 2) {
			printf("error: missing breakpoint address\n");
			return;
		}
		addr = (uint32_t)strtoul(argv[1], NULL, 10);
		LIST_FOREACH(bp, &breakpoint_list, entry) {
			if (bp->addr == addr)
				break;
		}
		if (bp != NULL) {
			printf("error: breakpoint already present\n");
			return;
		}
		/* TODO: insert instruction */
		bp = malloc(sizeof(*bp));
		if (bp == NULL) {
			printf("error: unable to allocate memory\n");
			return;
		}
		bp->n = last_breakpoint++;
		bp->addr = addr;
		pru_insert_breakpoint(pru, pru_number, addr, &bp->orig_instr);
		LIST_INSERT_HEAD(&breakpoint_list, bp, entry);
	} else
		printf("error: unknown breakpoint command\n");
}

static void
cmd_continue(int argc __unused, const char *argv[] __unused)
{
	if (in_breakpoint) {
		pru_write_imem(pru, pru_number,
		    in_breakpoint->addr, in_breakpoint->orig_instr);
		pru_enable(pru, pru_number, 1);
		pru_insert_breakpoint(pru, pru_number,
		    in_breakpoint->addr, NULL);
		cmd_run(0, NULL);
	} else {
		printf("error: not in a breakpoint\n");
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
	enum pru_reg reg;

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
				reg = reg_name_to_enum(argv[1]);
				if (reg == REG_INVALID) {
					printf("error: invalid register\n");
					return;
				}
				printf("  %s = 0x%x\n", argv[1],
				    pru_read_reg(pru, pru_number, reg));
			}
		} else
			printf("error: missing register name\n");
	} else if (strcmp(argv[0], "write") == 0) {
		if (argc > 2) {
			val = (uint32_t)strtoul(argv[2], NULL, 10);
			reg = reg_name_to_enum(argv[1]);
			if (reg == REG_INVALID) {
				printf("error: invalid register\n");
				return;
			}
			pru_write_reg(pru, pru_number, reg, val);
		} else
			printf("error: missing register and/or value\n");
	} else
		printf("error: unsupported sub-command\n");
}

static void
cmd_memory(int argc, const char *argv[])
{
	uint8_t *buf;
	uint32_t size, i, addr;

	if (argc == 0) {
		printf("The following sub-commands are supported:\n\n");
		printf("read  -- Read from the PRU memory.\n");
		printf("write -- Write to the PRU memory.\n");
		return;
	}
	if (strcmp(argv[0], "read") == 0) {
		if (argc > 1)
			addr = (uint32_t)strtoul(argv[1], NULL, 10);
		else
			addr = 0;
		if (argc > 2)
			size = (uint32_t)strtoul(argv[2], NULL, 10);
		else
			size = 128;
		buf = malloc(size);
		for (i = 0; i < size; i++) {
			buf[i] = pru_read_mem(pru, pru_number, addr + i);
		}
		hexdump(buf, (int)size, NULL, 0);
		free(buf);
	} else if (strcmp(argv[0], "write") == 0) {
		/* TODO */
	} else
		printf("error: unsupported sub-command\n");
}

static void
cmd_step(int argc __unused, const char *argv[] __unused)
{
	pru_enable(pru, pru_number, 1);
}


static unsigned char
complete(EditLine *cel, int ch __unused)
{
        const LineInfo *lf;
	size_t len, i, lastidx;
	char line[32];
	int matches;

	lf = el_line(cel);
        len = (size_t)(lf->lastchar - lf->buffer);
	if (len >= sizeof(line))
		return (CC_ERROR);
	strlcpy(line, lf->buffer, len + 1);
	if (strchr(line, ' ') != NULL)
		return CC_ERROR;

	for (lastidx = matches = i = 0; i < nitems(cmds); i++) {
		if (strncmp(line, cmds[i].cmd, len) == 0) {
			matches++;
			lastidx = i;
		}
	}
	if (matches == 0 || matches > 1)
		return CC_NORM;

	el_insertstr(cel, cmds[lastidx].cmd + len);
	el_insertstr(cel, " ");

	return CC_REFRESH;
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
	el_set(elp, EL_ADDFN, "prudbg-complete",
	    "Line completion", complete);
	el_set(elp, EL_BIND, "^I", "prudbg-complete", NULL);
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
	int ch, attach;
	char *type;
	pru_type_t pru_type;

	attach = 0;
	type = NULL;
	while ((ch = getopt(argc, argv, "ap:t:")) != -1) {
		switch (ch) {
		case 'a':
			attach = 1;
			break;
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
	if (argc == 0 && !attach) {
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
	if (!attach) {
		pru_reset(pru, pru_number);
		printf("Uploading '%s' to PRU %d: ", argv[0], pru_number);
		fflush(stdout);
		if (pru_upload(pru, pru_number, argv[0]) != 0) {
			printf("\n");
			pru_free(pru);
			err(1, "could not upload file");
		}
		printf("done.\n");
	} else
		printf("Attaching to PRU %d.\n", pru_number);

	return main_interface();
}
