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

#include "jackclient.h"
#include "jpmidi.h"
#include "main.h"

static int warn_if_not_connected = 1;
static jack_position_t transport_pos;

/* This is the frame we exepect in the next process cycle if we don't
 * need to seek within our own data and the transport position has not
 * been relocated.
 */
static jack_nframes_t expected_frame = UINT32_MAX;

/* Pointer to the next time/events to play. */
static jpmidi_time_t* current_time = NULL;

static jack_client_t *client;
static jack_port_t *output_port;


// transport state during the previous cycle
static jack_transport_state_t prev_state = JackTransportStopped;

int process(jack_nframes_t nframes, void *arg); // forward declaration
void jackclient_cm_setup();
void jackclient_cm_process_init();
void jackclient_control_message_return( control_message_t* message);
control_message_t* jackclient_cm_process_next();

int jackclient_new(const char* client_name)
{
    jackclient_cm_setup();
    
    if((client = jack_client_new ( client_name)) == 0)
    {
        fprintf (stderr, "jack server not running?\n");
        return 1;
    }
    jack_set_process_callback (client, process, 0);
    output_port = jack_port_register (client, "out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);

    if (output_port == NULL) {
        fprintf( stderr, "failed to create output port");
        jack_client_close(client);
        return 1;
    }

    return 0;
}

int jackclient_activate()
{
    if (jack_activate(client))
    {
        fprintf (stderr, "cannot activate jack client\n");
        return 1;
    }
    return 0;
}

int jackclient_deactivate()
{
    if (jack_deactivate(client)) {
        fprintf(stderr, "cannot deactivate jack client\n");
        return 1;
    }
    return 0;
}

int jackclient_close()
{
    
    if (jack_port_unregister( client, output_port)) {
        fprintf( stderr, "cannot unregister port\n");
        return 1;
	}
    if (jack_client_close(client)) {
        fprintf( stderr, "cannot close jack client\n");
        return 1;
    }
    return 0;
}

/** jpmidi's jack client process() thread logic. */
int process(jack_nframes_t nframes, void *arg)
{
    jpmidi_root_t* root = main_get_jpmidi_root();
    
 	void* port_buf = jack_port_get_buffer(output_port, nframes);
	jack_midi_clear_buffer(port_buf);
    
    jack_transport_state_t state = jack_transport_query (client, &transport_pos);
    
    if (state == JackTransportStopped) {
        warn_if_not_connected = 1;
    }
    else {
        if (warn_if_not_connected) {
            warn_if_not_connected = 0;
            if (output_port && !jack_port_connected (output_port))
            {
                fprintf( stderr, "\n\n*** jpmidi:out is not connected to anything!\n\n");
            }
        }

    }

    /** Send unscheduled control messages. */
    jackclient_cm_process_init();
    control_message_t* cm = jackclient_cm_process_next();
    while (cm != NULL)
    {
        unsigned char* buffer = jack_midi_event_reserve(port_buf, 0, cm->len);
        int j;
        for (j = 0; j < cm->len; j++) {
            buffer[j] = cm->data[j];
        }
        jackclient_control_message_return( cm);
        cm = jackclient_cm_process_next();
    }


    
    if (prev_state == JackTransportRolling && state == JackTransportStopped)
    {
        // Send all sound off controller messages on every channel
        int i;
        for (i = 0; i < 16; i++) {
            unsigned char* buffer = jack_midi_event_reserve(port_buf, 0, 3);
            buffer[0] = 0xB0 | i;
            buffer[1] = 120;
            buffer[2] = 0;
        }
    }

    prev_state = state;
    
    if (state != JackTransportRolling) return 0; // We don't do anything if the transport is not rolling.

    // Do we need to seek within our own midi data to sync the playback position?
    if (current_time == NULL || expected_frame != transport_pos.frame)
    {
        // We have an entry point every second.  Lookup time for the nearest previous second boundary.
        current_time = jpmidi_lookup_entrypoint( root, transport_pos.frame);

        // Advance to the time greater than or equal to the transport frame time.
        while (current_time && jpmidi_time_get_frame(current_time) < transport_pos.frame)
            current_time = jpmidi_time_get_next( current_time);
    }

    if (current_time == NULL) return 0; // Transport is beyond the last time in our own midi data.

    // Frame for the beginning of next cycle.  Events we send in this
    // cycle must have a frame time less than this.
    expected_frame = transport_pos.frame + nframes;

    if (jpmidi_time_get_frame(current_time) >= expected_frame) return 0; // no events to send this cycle

    while (current_time && jpmidi_time_get_frame(current_time) < expected_frame)
    {
        int i;
        for (i = 0; i < jpmidi_time_get_event_count(current_time); i++)
        {
            int j;
            jpmidi_event_t* event = jpmidi_time_get_event(current_time, i);

            // Apply sysex/solo/mute filters here

            if (jpmidi_event_is_sysex(event))
            {
                if (!jpmidi_is_send_sysex_enabled(root)) continue;
            }
            else {
                int channel = jpmidi_event_get_channel(event);
                if (jpmidi_get_solo_channel(root) != -1 && channel != jpmidi_get_solo_channel(root)) continue;
                if (jpmidi_channel_is_muted( root, channel)) continue;

                if (jpmidi_event_get_status( event) == 0x80 && channel == 9) continue; // no note off on channel 10 (fluidsynth workaround)
            }

            jack_nframes_t time_in_cycle = jpmidi_time_get_frame(current_time) - transport_pos.frame;
            unsigned char* buffer = jack_midi_event_reserve(port_buf, time_in_cycle, jpmidi_event_get_data_length(event));

            unsigned char* data = jpmidi_event_get_data( event);
            for (j = 0; j < jpmidi_event_get_data_length(event); j++)
                buffer[j] = data[j];
        }

        current_time = jpmidi_time_get_next( current_time);
    }
    
    return 0;
}

jack_client_t* jackclient_get_client()
{
    return client;
}
jack_port_t* jackclient_get_port()
{
    return output_port;
}

#define CM_POOL_SIZE 16

static int cm_setup = 0; ///< Boolean for one-time setup control.

static control_message_t  cm_pool_supply[CM_POOL_SIZE];
static control_message_t* cm_available[ CM_POOL_SIZE];
static control_message_t* cm_process[ CM_POOL_SIZE];

/** Reserve a control message.  There is a fixed number of messages
 *  available in the internal pool.  This function returns NULL if none
 *  are available.
 */
control_message_t* jackclient_control_message_reserve()
{
    control_message_t* result = NULL;
    int i;
    for (i = 0; i < CM_POOL_SIZE; i++)
    {
        if (cm_available[i] != NULL)
        {
            result = cm_available[i];
            cm_available[i] = NULL;
            return result;
        }
    }
    return NULL;
}


/** Schedules the control message to be sent on frame 0 of the next
 * process cycle and returns the message to the pool.
 */
void jackclient_control_message_queue( control_message_t* message)
{
    int i;
    for (i = 0; i < CM_POOL_SIZE; i++)
    {
        if (cm_process[i] == NULL)
        {
            cm_process[i] = message;
            return;
        }
    }
}

/////////// INTERNAL USE ONLY ///////////////////

/** Setup the cm message system. */
void jackclient_cm_setup()
{
    if (cm_setup) return;
    cm_setup = 1;
    int i;
    for (i = 0; i < CM_POOL_SIZE; i++) {
        cm_available[i] = &cm_pool_supply[i];
    }
}    
    
/** Return the message to the available pool. */
void jackclient_control_message_return( control_message_t* message)
{
    int i;
    for (i = 0; i < CM_POOL_SIZE; i++)
    {
        if (cm_available[i] == NULL)
        {
            cm_available[i] = message;
            return;
        }
    }
}

static int cm_process_index;

/** Call this as step 1 in handling the unscheduled control messages. */
void jackclient_cm_process_init()
{
    cm_process_index = 0;
}

/** Keep calling this until it returns NULL.  For each message, send it. */
control_message_t* jackclient_cm_process_next()
{
    control_message_t* result = NULL;
    
    while (cm_process_index < CM_POOL_SIZE)
    {
        if (cm_process[ cm_process_index] != NULL) {
            result = cm_process[ cm_process_index];
            cm_process[ cm_process_index] = NULL;
        }
        cm_process_index += 1;
        if (result != NULL) return result;
    }
    return result;
}
