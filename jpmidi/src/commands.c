
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string.h>
#include <jack/jack.h>
#include <jack/transport.h>

#include "dump.h"
#include "jpmidi.h"
#include "main.h"
#include "jackclient.h"
#include "commands.h"
#include "elements.h"

static jpmidi_time_t* last_dump_time = NULL;
static int64_t last_dump_count = -1;

static char* client_disabled_message = "Jack client disabled.";

/* forward declarations */
void com_help(char *);
void com_exit(char *arg);
void com_channels(char* arg);
void com_status(char* arg);
void com_sysex(char* arg);
void com_solo(char* arg);
void com_mute(char* arg);
void com_unmute(char* arg);
void com_dump(char* arg);
void connect_util( char* arg, int disconnect, char* verb, char* direction);
void com_connect( char* arg);
void com_disconnect( char* arg);
void com_play(char *arg);
void com_stop(char *arg);
void com_locate(char *arg);
command_t *find_command(char *name);

/* command table must be in alphabetical order */
command_t commands[] = {
    {"connect",     com_connect,    "Connect to port [port num].  List if no arg is given"},
    {"disconnect",  com_disconnect, "Disconnect from port [port num]"},
    {"status",      com_status,     "Display status"},
    {"channels",    com_channels,   "Display channel info"},
    {"sysex",       com_sysex,      "Enable or disable sending of sysex messages <0|1>"},
    {"solo",        com_solo,       "Solo channel <0 | 1-16>.  0 disables solo"},
    {"mute",        com_mute,       "Mute channel <1-16>"},
    {"unmute",      com_unmute,     "Unmute channel <1-16>"},
    {"play",        com_play,       "Start transport rolling"},
    {"start",       com_play,       "Start transport rolling"},
    {"stop",        com_stop,       "Stop transport"},
    {"locate",      com_locate,     "Locate to frame <position>"},
    {"dump",        com_dump,       "Dump event info [tick count] [start tick]"},
    {"exit",        com_exit,       "Exit jpmidi"},
    {"quit",        com_exit,       "Quit jpmidi"},
    {"help",        com_help,       "Display help text [<command>]"},
    {(char *)NULL, (cmd_function_t *)NULL, (char *)NULL }
};

/* ---- Command functions ---- */

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

void com_exit(char *arg)
{
	//done = 1;
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

/* ---- Command utility functions ---- */
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

int execute_command(char *line)
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

	/* Return 1 to exit command line mode */
	if ((strcmp(word,"quit") && strcmp(word,"exit")==0)) {
		return 1;
	}
     
	/* Get argument to command, if any. */
	while (whitespace(line[i]))
		i++;
     
	word = line + i;
     
	/* invoke the command function. */
	(*command->func)(word);

	return 0;
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
