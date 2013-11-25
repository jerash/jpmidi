/*
 * 
 * Copyright (C) 2007 Ken Ellinwood.
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 */
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>

#include "jpmidi.h"
#include "cmdline.h"
#include "tcpserver.h"
#include "dump.h"
#include "main.h"
#include "jackclient.h"

/* Options for the command */
#define HAS_ARG 1
static struct option long_opts[] = {
    {"version", 0, NULL, 'v'},
    {"disable-client", 0, NULL, 'd'},
    {"server", 0, NULL, 's'},
    {0, 0, 0, 0},
};


/* Number of elements in an array */
#define NELEM(a) ( sizeof(a)/sizeof((a)[0]) )

static int be_jack_client = 1;
static int be_server = 0;
static int TCPPORT = 2013;
static jpmidi_root_t* root;

int main_is_jack_client()
{
    return be_jack_client;
}

jpmidi_root_t* main_get_jpmidi_root()
{
    return root;
}

int main(int argc, char **argv)
{
    char opts[NELEM(long_opts) * 3 + 1];
    char *cp;
    int  c;
    struct option *op;

    /* Build up the short option string */
    cp = opts;
    for (op = long_opts; op < &long_opts[NELEM(long_opts)]; op++) {
        *cp++ = op->val;
        if (op->has_arg)
            *cp++ = ':';
    }


    /* Deal with the options */
    for (;;) {
        c = getopt_long(argc, argv, opts, long_opts, NULL);
        if (c == -1)
            break;

        switch(c) {
        case 'v':
            main_showversion();
            exit(0);
        case 'd':
            be_jack_client = 0;
            break;
	case 's':
	    be_server = 1;
	    break;
        default:
            main_showusage();
            exit(1);
        }
    }

    if (optind >= argc) {
        printf("No input files!\n");
        main_showusage();
        exit( 1);
    }

    if (optind < argc-1) {
        printf("Too many input files!\n");
        main_showusage();
        exit( 1);
    }

    if (!jpmidi_init()) {
        fprintf( stderr, "Failed to initialize jpmidi, errno = %d\n", errno);
        exit( errno);
    }

    dump_init();

    jack_nframes_t jack_sample_rate = 44100;
    
    if (be_jack_client)
    {
        if (jackclient_new( "jpmidi")) return 1;
        
        jack_sample_rate = jack_get_sample_rate(jackclient_get_client());
    }
    else printf("Not connecting to jack, assuming sample rate of %d\n", jack_sample_rate);


    root = jpmidi_loadfile(argv[optind], jack_sample_rate);
    printf("loaded %s\n", root->filename);

    if (be_jack_client && jackclient_activate()) return 1;
    
    /* launch either command line or server mode */
    if (be_server)
    {
	tcpserver(TCPPORT);
    }
    else
    {
	cmdline();
    }

    if (be_jack_client && jackclient_deactivate()) return 1;
    if (be_jack_client && jackclient_close()) return 1;
    
    return 0;
}

/*
 * Show a usage message
 */
void main_showusage()
{
    char **cpp;
    static char *msg[] = {
        "Usage: jpmidi [options] midi-file",
        "OPTIONS:",
        "    --version or -v               - Show program version",
        "    --disable-client or -d        - Dont connect as a jack client",
	"    --server or -s                - wait commands on TCP port 2013",
    };

    for (cpp = msg; cpp < msg+NELEM(msg); cpp++) {
        fprintf(stderr, "%s\n", *cpp);
    }
}

void main_showversion()
{
    printf("jpmidi-%s\n", VERSION);
}


