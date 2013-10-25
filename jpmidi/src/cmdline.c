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

/* includes for server mode */
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
/* end include for server mode */

#include "dump.h"
#include "jpmidi.h"
#include "main.h"
#include "jackclient.h"
#include "elements.h"

static char *package = "jpmidi";				/* program name */
int done = 0;

static char* client_disabled_message = "Jack client disabled.";

void signal_handler(int sig)
{
	fprintf(stderr, "signal received, exiting ...\n");
	exit(0);
}


/* Command functions: see commands[] table following. */

void com_exit(char *arg)
{
	done = 1;
}

void com_channels(char* arg)
{
    int i;

    jpmidi_root_t* root = main_get_jpmidi_root();
    if (arg == NULL) arg = "";

    printf("%sSolo: ", arg);
    if (jpmidi_get_solo_channel( root) == -1) printf("off\n");
    else printf("channel %d\n", jpmidi_get_solo_channel( root)+1);
    for (i = 0; i < 16; i++) {
        if (!jpmidi_channel_has_data( root, i)) continue;
        printf("%schannel %2d, muted: %d, program: %s\n",
               arg,
               jpmidi_channel_get_number( root, i),
               jpmidi_channel_is_muted( root, i),
               jpmidi_channel_get_program( root,i));
    }
}

void com_status(char* arg)
{
    // Display some statistics
    jpmidi_root_t* root = main_get_jpmidi_root();
    jpmidi_time_t* node = jpmidi_get_time_head( root);
    uint32_t ecount = 0;
    uint32_t tcount = 0;
    while (node) {
        ecount += jpmidi_time_get_event_count(node);
        tcount += 1;
        node = jpmidi_time_get_next(node);
    }

    printf("MIDI file: %s\n", jpmidi_get_filename( root));
    printf("   SMF timebase: %u\n", jpmidi_get_smf_timebase(root));
    printf("%u events, %u time records\n", ecount, tcount);
    printf("Channels:\n");
    com_channels("   ");
    printf("Send sysex:   %s\n", jpmidi_is_send_sysex_enabled( root) ? "on" : "off");
    
    if (main_is_jack_client()) {
        printf("JACK port: %s\n", jack_port_name ( jackclient_get_port()));
        printf("   %d connections", jack_port_connected (jackclient_get_port()));
        const char** conns = jack_port_get_connections (jackclient_get_port());
        if (conns) {
            int i;
            for (i = 0; conns[i]; i++) {
                if (i == 0) printf(": ");
                else printf(", ");
                printf("%s", conns[i]);
            }
            free( conns);
        }
        printf("\n");
        
        jack_position_t transport_pos;
        jack_transport_state_t state = jack_transport_query (jackclient_get_client(), &transport_pos);
        
        char* state_str;
        switch (state) {
        case JackTransportStopped:
            state_str = "stopped";
            break;
        case JackTransportRolling:
            state_str = "rolling";
            break;
        case JackTransportLooping:
            state_str = "looping";
            break;
        case JackTransportStarting:
            state_str = "starting";
            break;
        default:
            state_str = "unknown";
            break;
        }
        
        printf("Transport state: %s\n", state_str);
        printf("Transport position, frame: %u\n", transport_pos.frame);
    }
    else printf("%s\n", client_disabled_message);
}


void com_sysex(char* arg)
{
    int enable = -1;
    jpmidi_root_t* root = main_get_jpmidi_root();

    if (strlen(arg) > 0) sscanf( arg, "%d", &enable);
    if (enable == -1)
    {
        printf("Invalid argument.  Usage: sysex <0|1>.  Use '0' to disable sending of system exclusive messages.\n");
        return;
    }

    jpmidi_set_send_sysex_enabled( root, enable != 0);
}

void com_solo(char* arg)
{
    int sc = -1;
    jpmidi_root_t* root = main_get_jpmidi_root();

    if (strlen(arg) > 0) sscanf( arg, "%d", &sc);
    if (sc < 0 || sc > 16)
    {
        printf("Invalid argument.  Usage: solo <0 | 1-16>.  Use '0' to disable solo\n");
        return;
    }

    jpmidi_solo_channel( root, sc);

    if (sc == 0) return;

    // Arrange for all sound off controller messages to be sent on the channels that aren't being solo'ed.
    sc = sc - 1;
    int i;
    for (i = 0; i < 16; i++)
    {
        if (i == sc) continue;
        if (jpmidi_channel_has_data(root, i))
        {
            control_message_t* cm = jackclient_control_message_reserve();
            if (cm == NULL) {
                return; // No more control messages left in the queue, not much to do
            }
            cm->len = 3;
            cm->data[0] = 0xB0 | i;
            cm->data[1] = 120;
            cm->data[2] = 0;
            jackclient_control_message_queue( cm);    
        }
    }

}

void com_mute(char* arg)
{
    int mc = -1;
    jpmidi_root_t* root = main_get_jpmidi_root();

    if (strlen(arg) > 0) sscanf( arg, "%d", &mc);
    if (mc < 1 || mc > 16)
    {
        printf("Invalid argument.  Usage: mute <1-16>\n");
        return;
    }

    jpmidi_mute_channel( root, mc);

    // Arrange for all sound off controller messages to be sent on the channel just muted.
    mc = mc - 1;
    control_message_t* cm = jackclient_control_message_reserve();
    if (cm == NULL) return; // No more control messages left in the queue, not much to do
    cm->len = 3;
    cm->data[0] = 0xB0 | mc;
    cm->data[1] = 120;
    cm->data[2] = 0;
    jackclient_control_message_queue( cm);    
}

void com_unmute(char* arg)
{
    int mc = -1;
    jpmidi_root_t* root = main_get_jpmidi_root();

    if (strlen(arg) > 0) sscanf( arg, "%d", &mc);
    if (mc < 1 || mc > 16)
    {
        printf("Invalid argument.  Usage: unmute <1-16>\n");
        return;
    }
    
    jpmidi_unmute_channel( root, mc);
}

static jpmidi_time_t* last_dump_time = NULL;
static int64_t last_dump_count = -1;

void com_dump(char* arg)
{
    
    int64_t count = -1;
    int64_t tick = -1;
    jpmidi_root_t* root = main_get_jpmidi_root();

    if (strlen(arg) > 0) sscanf( arg, "%lld %lld", &count, &tick);

    // printf("count: %lld, tick: %lld\n", count, tick);
    
    if (count < 0) {
        if (last_dump_count > 0) count = last_dump_count;
        else count = 10;
    }
    else last_dump_count = count;

    jpmidi_time_t* time = jpmidi_get_time_head( root);
    
    if (tick > 0 || last_dump_time == NULL)
    {

        if (tick < 0) tick = 0;
        
        while (time && jpmidi_time_get_smf_time(time) < tick) time = jpmidi_time_get_next( time);
        
        if (time == NULL) return;        
    }
    else time = last_dump_time;

    last_dump_time = jpmidi_dump( time, (uint32_t)count, 0);
}

void connect_util( char* arg, int disconnect, char* verb, char* direction)
{
    if (!main_is_jack_client()) {
        printf("%s\n", client_disabled_message);
        return;
    }
        
    const char** ports = jack_get_ports (jackclient_get_client(), NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput);
    if (ports == NULL) return;

    const char** conns = jack_port_get_connections (jackclient_get_port());
    
    int index = -1;

    sscanf( arg, "%d", &index);

    if (index == -1)
    {
        printf("Destination ports:\n");
        int i;
        for (i = 0; ports[i]; ++i) {
            printf("%d) %s ", i+1, ports[i]);
            if (conns) {
                int j;
                for (j = 0; conns[j]; j++) {
                    if (strcmp( conns[j], ports[i]) == 0) printf(" (connection established)");
                }
            }
            printf("\n");
        }
    }
    else {

        int i;
        for (i = 0; ports[i]; ++i) {
        }

        if (index < 1 || index > i) {
            printf("Usage: %s <port number>\n", verb);
            free(ports);
            return;
        }

        int result;

        if (disconnect) result = jack_disconnect( jackclient_get_client(), jack_port_name (jackclient_get_port()), ports[index-1]);
        else result = jack_connect( jackclient_get_client(), jack_port_name (jackclient_get_port()), ports[index-1]);
        if (result)
            printf("Failed to %s %s %s\n", verb, direction, ports[index-1]);
        else
            printf("Successfully %sed %s %s\n", verb, direction, ports[index-1]);
    }
        
    free( ports);
}


void com_connect( char* arg)
{
    connect_util( arg, 0, "connect", "to");
}

void com_disconnect( char* arg)
{
    connect_util( arg, 1, "disconnect", "from");
}

void com_play(char *arg)
{
    if (!main_is_jack_client()) {
        printf("%s\n", client_disabled_message);
        return;
    }
	jack_transport_start(jackclient_get_client());
}


void com_stop(char *arg)
{
    if (!main_is_jack_client()) {
        printf("%s\n", client_disabled_message);
        return;
    }
	jack_transport_stop(jackclient_get_client());
}

void com_locate(char *arg)
{
    if (!main_is_jack_client()) {
        printf("%s\n", client_disabled_message);
        return;
    }
	jack_nframes_t frame = 0;

	if (*arg != '\0')
		frame = atoi(arg);

	jack_transport_locate(jackclient_get_client(), frame);
}

void com_help(char *);			/* forward declaration */

/* Command parsing based on GNU readline info examples. */

typedef void cmd_function_t(char *);	/* command function type */

/* Transport command table. */
typedef struct {
	char *name;			/* user printable name */
	cmd_function_t *func;		/* function to call */
	char *doc;			/* documentation  */
} command_t;

/* command table must be in alphabetical order */
command_t commands[] = {
    {"connect",	    com_connect,    "Connect to port [port num].  List if no arg is given"},
    {"disconnect",	com_disconnect, "Disconnect from port [port num]"},
    {"status",	    com_status,	    "Display status"},
    {"channels",    com_channels,   "Display channel info"},
    {"sysex",       com_sysex,      "Enable or disable sending of sysex messages <0|1>"},
    {"solo",        com_solo,       "Solo channel <0 | 1-16>.  0 disables solo"},
    {"mute",        com_mute,       "Mute channel <1-16>"},
    {"unmute",      com_unmute,     "Unmute channel <1-16>"},
	{"play",	    com_play,	    "Start transport rolling"},
	{"stop",	    com_stop,	    "Stop transport"},
	{"locate",	    com_locate,	    "Locate to frame <position>"},
    {"dump",	    com_dump,	    "Dump event info [tick count] [start tick]"},
	{"exit",	    com_exit,	    "Exit jpmidi"},
	{"quit",	    com_exit,	    "Quit jpmidi"},
	{"help",	    com_help,	    "Display help text [<command>]"},
	{(char *)NULL, (cmd_function_t *)NULL, (char *)NULL }
};
     
command_t *find_command(char *name)
{
	register int i;
	size_t namelen;

	if ((name == NULL) || (*name == '\0'))
		return ((command_t *)NULL);

	namelen = strlen(name);
	for (i = 0; commands[i].name; i++)
		if (strncmp(name, commands[i].name, namelen) == 0) {

			/* make sure the match is unique */
			if ((commands[i+1].name) &&
			    (strncmp(name, commands[i+1].name, namelen) == 0))
				return ((command_t *)NULL);
			else
				return (&commands[i]);
		}
     
	return ((command_t *)NULL);
}

void com_help(char *arg)
{
	register int i;
	command_t *cmd;

	if (!*arg) {
		/* print help for all commands */
		for (i = 0; commands[i].name; i++) {
			printf("%-12s\t%s.\n", commands[i].name,
			       commands[i].doc);
		}

	} else if ((cmd = find_command(arg))) {
		printf("-12%s\t%s.\n", cmd->name, cmd->doc);

	} else {
		int printed = 0;

		printf("No `%s' command.  Valid command names are:\n", arg);

		for (i = 0; commands[i].name; i++) {
			/* Print in six columns. */
			if (printed == 6) {
				printed = 0;
				printf ("\n");
			}

			printf ("%s\t", commands[i].name);
			printed++;
		}

		printf("\n\nTry `help [command]\' for more information.\n");
	}
}

void execute_command(char *line)
{
	register int i;
	command_t *command;
	char *word;
     
	/* Isolate the command word. */
	i = 0;
	while (line[i] && whitespace(line[i]))
		i++;
	word = line + i;
     
	while (line[i] && !whitespace(line[i]))
		i++;
     
	if (line[i])
		line[i++] = '\0';
     
	command = find_command(word);
     
	if (!command) {
		fprintf(stderr, "%s: No such command.  There is `help\'.\n",
			word);
		return;
	}
     
	/* Get argument to command, if any. */
	while (whitespace(line[i]))
		i++;
     
	word = line + i;
     
	/* invoke the command function. */
	(*command->func)(word);
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
     
char *dupstr(char *s)
{
	char *r = malloc(strlen(s) + 1);
	strcpy(r, s);
	return r;
}
     
/* Readline generator function for command completion. */
char *command_generator (const char *text, int state)
{
	static int list_index, len;
	char *name;
     
	/* If this is a new word to complete, initialize now.  This
	   includes saving the length of TEXT for efficiency, and
	   initializing the index variable to 0. */
	if (!state) {
		list_index = 0;
		len = strlen (text);
	}
     
	/* Return the next name which partially matches from the
	   command list. */
	while ((name = commands[list_index].name)) {
		list_index++;
     
		if (strncmp(name, text, len) == 0)
			return dupstr(name);
	}
     
	return (char *) NULL;		/* No names matched. */
}

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
			execute_command(cmd);
		}
     
		free(line);		/* realine() called malloc() */
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

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void dostuff (int sock)
{
   int n;
   char buffer[256];
   char *cmd;
      
   bzero(buffer,256);
   n = read(sock,buffer,255);
   if (n < 0) perror("ERROR reading from socket");

   // remove \n
   strstr(buffer, "\n");
   while ((cmd = strstr(buffer, "\n")) != NULL) {
     int len = strlen(buffer);
     memmove(cmd, cmd + 1, len); 
   }

   /* execute command */
   //printf("Received command: %s\n",buffer);
   add_history(buffer);
   execute_command(buffer);

   /* send ack */
   n = write(sock,"ack",3);
   if (n < 0) perror("ERROR writing to socket");
}

void tcpserver(int tcp_port)
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;

     printf("--Server Mode--\nListening on port %i\n",tcp_port);

     if (tcp_port == 0) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        perror("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = tcp_port;
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              perror("ERROR on binding");
     listen(sockfd,5);
     clilen = sizeof(cli_addr);
     while (1) {
         newsockfd = accept(sockfd, 
               (struct sockaddr *) &cli_addr, &clilen);
         if (newsockfd < 0) 
             perror("ERROR on accept");
         pid = fork();
         if (pid < 0)
             perror("ERROR on fork");
         if (pid == 0)  {
             close(sockfd);
	     // redirect stdout to socket
	     close(1);
	     dup(newsockfd);
	     //ready to serve
             dostuff(newsockfd);
             exit(0);
         }
         else close(newsockfd);
     } /* end of while */
     close(sockfd);
}
