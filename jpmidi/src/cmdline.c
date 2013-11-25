/*
 *  cmdline.c - command line interface for jpmidi
 *  Based on transport.c the JACK transport master example client.
 *  Copyright (C) 2003 Jack O'Quin.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <jack/jack.h>
#include <jack/transport.h>

#include "dump.h"
#include "jpmidi.h"
#include "main.h"
#include "jackclient.h"
#include "commands.h"
#include "elements.h"

static char *package = "jpmidi";				/* program name */
int done = 0;

void signal_handler(int sig)
{
	fprintf(stderr, "signal received, exiting ...\n");
	exit(0);
}

/* Strip whitespace from the start and end of string. */
char *stripwhite(char *string)
{
	register char *s, *t;

	s = string;
	while (whitespace(*s))
		s++;

	if (*s == '\0')
		return s;
     
	t = s + strlen (s) - 1;
	while (t > s && whitespace(*t))
		t--;
	*++t = '\0';
     
	return s;
}
     
/*--------------CMDLINE-----------------------*/

void command_loop()
{
	char *line, *cmd;
	char prompt[32];

	snprintf(prompt, sizeof(prompt), "%s> ", package);

	/* Allow conditional parsing of the ~/.inputrc file. */
	rl_readline_name = package;
     
	/* Define a custom completion function. */
	rl_completion_entry_function = command_generator;

	/* Read and execute commands until the user quits. */
	while (!done) {

		line = readline(prompt);
     
		if (line == NULL) {	/* EOF? */
			printf("\n");	/* close out prompt */
			done = 1;
			break;
		}
     
		/* Remove leading and trailing whitespace from the line. */
		cmd = stripwhite(line);

		/* If anything left, add to history and execute it. */
		if (*cmd)
		{
			add_history(cmd);
			if(execute_command(cmd))
			{
			  done = 1;
			}
		}
     
		free(line);		/* readline() called malloc() */
	}
}

void cmdline()
{

	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);

	/* execute commands until done */
	command_loop();

}
