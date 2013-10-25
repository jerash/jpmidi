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

/* jpmidi - Plays a midi file using Jack (MIDI/Transport APIs).
 */

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "jpmidi.h"
#include "dump.h"
#include "elements.h"
#include "except.h"
#include "md.h"
#include "midi.h"

/* Private functions */

/* Process the element created during SMF parse. */
void jpmidi_process_element(jpmidi_root_t*, struct element *el);

gboolean jpmidi_root_data_traverse(gpointer key, gpointer value, gpointer data);

static GArray* listeners;

int jpmidi_init()
{
    dump_init();
    listeners = g_array_new( FALSE, FALSE, sizeof( jpmidi_loadfile_listener_t));
    return 1; // Success
}

/* Invoke the loadfile listener callbacks.  The value of root is NULL when the previous file has just been unloaded from memory. */
void jpmidi_call_loadfile_listeners( jpmidi_root_t* root)
{
    int i;
    for (i = 0; i < listeners->len; i++) {
        jpmidi_loadfile_listener_t callback = g_array_index( listeners, jpmidi_loadfile_listener_t, i);
        (*callback)(root);
    }
}

/* Add a listener. */
void jpmidi_add_loadfile_listener( jpmidi_loadfile_listener_t callback)
{
    g_array_append_val( listeners, callback);
}
    
/* Remove a listener. */    
void jpmidi_remove_loadfile_listener( jpmidi_loadfile_listener_t callback)
{
    int i;
    int remove_index = -1;
    for (i = 0; i < listeners->len; i++) {
        jpmidi_loadfile_listener_t c = g_array_index( listeners, jpmidi_loadfile_listener_t, i);
        if (c == callback) {
            remove_index = i;
            break;
        }
    }
    if (remove_index >= 0) g_array_remove_index( listeners, remove_index);
}

/*
 * Load/process a MIDI file and get ready to play it via Jack's MIDI API.
 */
jpmidi_root_t* jpmidi_loadfile(char *filename, jack_nframes_t sample_rate)
{
    struct rootElement *proot;
    struct sequenceState *seq;
    struct element *el;

    if (strcmp(filename, "-") == 0)
        proot = midi_read(stdin);
    else
        proot = midi_read_file(filename);
    if (!proot)
        return NULL;


    /* FIXME - free this somewhere */
    jpmidi_root_t* root = jpmidi_root_new( filename, proot, sample_rate);
    
    /* Process all the elements in the file. */
    seq = md_sequence_init(proot);
    while ((el = md_sequence_next(seq)) != NULL) {
        jpmidi_process_element(root, el);
    }

    // Add an entry point every second to reduce seek time
    jack_nframes_t epframe;

    for (epframe = 0; epframe < root->last_frame; epframe += root->sample_rate)
    {
        jpmidi_time_t* time = (jpmidi_time_t*)g_tree_lookup( root->data, &epframe);
        if (time == NULL) {
            time = jpmidi_time_new( 0, epframe);
            g_tree_insert( root->data, &time->frame, time);
    }
    }
    
    // Create the linked list of time structs ordered by time.
    root->head = NULL;
    root->tail = NULL;
    g_tree_foreach( root->data, jpmidi_root_data_traverse, root);


    jpmidi_call_loadfile_listeners( root);

    return root;
}

/** Traversal function for g_tree_foreach().  This function links the
 *  time structs in root->data (GTree) into a list ordered by time.
 */
gboolean jpmidi_root_data_traverse(gpointer key, gpointer value, gpointer data)
{
    jpmidi_root_t* root = (jpmidi_root_t*)data;
    jpmidi_time_t* curr_time = (jpmidi_time_t*)value;
    
    if (root->head == NULL) root->head = curr_time;
    if (root->tail != NULL) root->tail->next_time = curr_time;

    curr_time->next_time = NULL;
    root->tail = curr_time;
    
    return FALSE;
}

/** GCompareFunc for comparing the jack_nframes_t* keys in the root data tree. */
gint jpmidi_time_compare(gconstpointer  a, gconstpointer  b)
{
    guint64 ta = (gint64)*(jack_nframes_t*)a;
    guint64 tb = (gint64)*(jack_nframes_t*)b;

    return (gint)(ta - tb);
    
}

/** Create a new root data structure. */
jpmidi_root_t* jpmidi_root_new( char* filename, struct rootElement* proot, jack_nframes_t sample_rate)
{
    jpmidi_root_t* root = g_new0( jpmidi_root_t, 1);
    root->filename = strdup( filename);
    root->pmidi_root = proot;
    root->sample_rate = sample_rate;
    root->tempo_mpq = 500000;        /* 500K microseconds per quarter note = 120 BPM */
    root->data = g_tree_new( jpmidi_time_compare);

    root->send_sysex = 1;
    root->solo_channel = -1;

    int i;
    for (i = 0; i < 16; i++) {
        root->channel[i].number = i+1;
    }
    return root;
}

/** Free the root data structure and everything in it. */
void jpmidi_root_free( jpmidi_root_t* root)
{
    md_free(MD_ELEMENT(root->pmidi_root));
    /** FIXME - free the time and event data. */
    g_tree_destroy( root->data);
    g_free( root);

}

/** Solo the specified channel.  Returns 0 on success, 1 otherwise. */
int jpmidi_solo_channel( jpmidi_root_t* root, int chan)
{
    if (root == NULL || chan < 0 || chan > 15) return 1;
    root->solo_channel = chan - 1;
    return 0;
}
    
/** Mute the specified channel.  Returns 0 on success, 1 otherwise. */
int jpmidi_mute_channel( jpmidi_root_t* root, int chan)
{
    if (root == NULL || chan < 0 || chan > 15) return 1;
    root->channel[chan-1].muted = 1;
    return 0;
}
    
/** Unmute the specified channel.  Returns 0 on success, 1 otherwise. */
int jpmidi_unmute_channel( jpmidi_root_t* root, int chan)
{
    if (root == NULL || chan < 0 || chan > 15) return 1;
    root->channel[chan-1].muted = 0;
    return 0;
}

/** Create a new jpmidi_time structure. */
jpmidi_time_t* jpmidi_time_new( uint32_t smf_time, jack_nframes_t frame)
{
    jpmidi_time_t* time = g_new0( jpmidi_time_t, 1);
    time->smf_time = smf_time;
    time->frame = frame;
    time->events = g_array_new( FALSE, FALSE, sizeof( jpmidi_event_t*));
    return time;
}

/** Returns a jpmidi_time_t* for the given SMF time.  This method creates one if it does not already exist. */
jpmidi_time_t* jpmidi_get_time( jpmidi_root_t* root, uint32_t smf_time)
{
    jack_nframes_t frame = root->xtempo_frame + (jack_nframes_t)(root->samples_per_tick * (smf_time - root->xtempo_tick));
    jpmidi_time_t* time = (jpmidi_time_t*)g_tree_lookup( root->data, &frame);
    if (time == NULL) {
        if (root->last_frame < frame) root->last_frame = frame;
        time = jpmidi_time_new( smf_time, frame);
        g_tree_insert( root->data, &time->frame, time);
    }
    return time;
    
}

void jpmidi_time_add_event( jpmidi_time_t* time, jpmidi_event_t* event)
{
    g_array_append_val( time->events, event);
}

void jpmidi_time_free( jpmidi_time_t* time)
{

    int i;
    for (i = 0; i < time->events->len; i++) {
        jpmidi_event_t* event = g_array_index(time->events, jpmidi_event_t*, i);
        jpmidi_event_free( event);
    }
    g_free( time);
}

jpmidi_event_t* jpmidi_event_new( struct element* element)
{
    jpmidi_event_t* event = g_new0( jpmidi_event_t, 1);
    event->element = element;
    event->data = g_byte_array_new();
    return event;
}

void jpmidi_event_free( jpmidi_event_t* event)
{
    g_byte_array_free( event->data, TRUE);
    g_free( event);
}


void jpmidi_process_element(jpmidi_root_t* root, struct element *el)
{
    
    switch (el->type) {
    case MD_TYPE_ROOT:
        root->time_base = MD_ROOT(el)->time_base;

        // ( sample rate * MPQ ) / ( 1000000 * TPQ ) 
        root->samples_per_tick = ((double)root->sample_rate * (double)root->tempo_mpq) / ( 1000000 * (double)root->time_base );
        return;
    case MD_TYPE_TEMPO:
        // Remember where the tempo changes in terms of jack frame and smf tick.
        root->xtempo_frame = root->xtempo_frame + (jack_nframes_t)(root->samples_per_tick * (el->element_time - root->xtempo_tick));
        root->xtempo_tick = el->element_time;

        // Update the samples per tick value.
        root->tempo_mpq = MD_TEMPO(el)->micro_tempo;
        root->samples_per_tick = ((double)root->sample_rate * (double)root->tempo_mpq) / ( 1000000 * (double)root->time_base );
        return;
    case MD_TYPE_NOTE:
    {
        root->channel[ el->device_channel].has_data = 1;
        /* Create the note-on event */
        jpmidi_time_t* on_time = jpmidi_get_time( root, el->element_time);
        jpmidi_event_t* on_event = jpmidi_event_new( el);
        jpmidi_time_add_event( on_time, on_event);
        uint8_t midi[3];
        midi[0] = 0x90 | (0x0F & el->device_channel);
        midi[1] = MD_NOTE(el)->note;
        midi[2] = MD_NOTE(el)->vel;

        g_byte_array_append( on_event->data, midi, 3);

        /* Create corresponding off event. */
        uint32_t off_tick = el->element_time + MD_NOTE(el)->length;
        jpmidi_time_t* off_time = jpmidi_get_time( root, off_tick);
        jpmidi_event_t* off_event = jpmidi_event_new( el);
        jpmidi_time_add_event( off_time, off_event);
        
        midi[0] = 0x80 | (0x0F & el->device_channel);
        midi[1] = MD_NOTE(el)->note;
        midi[2] = MD_NOTE(el)->offvel;

        g_byte_array_append( off_event->data, midi, 3);

        on_event->related = off_event;
        off_event->related = on_event;
        
        return;
    }
    case MD_TYPE_KEYTOUCH:
    {
        root->channel[ el->device_channel].has_data = 1;
        jpmidi_time_t* time = jpmidi_get_time( root, el->element_time);
        jpmidi_event_t* event = jpmidi_event_new( el);
        jpmidi_time_add_event( time, event);
        uint8_t midi[3];

        midi[0] = 0xA0 | (0x0F & el->device_channel);
        midi[1] = MD_KEYTOUCH(el)->note;
        midi[2] = MD_KEYTOUCH(el)->velocity;

        g_byte_array_append( event->data, midi, 3);

        return;
    }
    case MD_TYPE_CONTROL:
    {
        root->channel[ el->device_channel].has_data = 1;
        jpmidi_time_t* time = jpmidi_get_time( root, el->element_time);
        jpmidi_event_t* event = jpmidi_event_new( el);
        jpmidi_time_add_event( time, event);
        uint8_t midi[3];

        midi[0] = 0xB0 | (0x0F & el->device_channel);
        midi[1] = MD_CONTROL(el)->control; 
        midi[2] = MD_CONTROL(el)->value;

        g_byte_array_append( event->data, midi, 3);

        return;
               }
    case MD_TYPE_PROGRAM:
    {
        root->channel[ el->device_channel].has_data = 1;
        if (root->channel[ el->device_channel].program == NULL)
            root->channel[ el->device_channel].program = dump_get_program_description( MD_PROGRAM(el)->program);
        
        jpmidi_time_t* time = jpmidi_get_time( root, el->element_time);
        jpmidi_event_t* event = jpmidi_event_new( el);
        jpmidi_time_add_event( time, event);
        uint8_t midi[2];

        midi[0] = 0xC0 | (0x0F & el->device_channel);
        midi[1] = MD_PROGRAM(el)->program;

        g_byte_array_append( event->data, midi, 2);

        return;
    }
    case MD_TYPE_PRESSURE:
    {
        root->channel[ el->device_channel].has_data = 1;
        jpmidi_time_t* time = jpmidi_get_time( root, el->element_time);
        jpmidi_event_t* event = jpmidi_event_new( el);
        jpmidi_time_add_event( time, event);
        uint8_t midi[2];

        midi[0] = 0xD0 | (0x0F & el->device_channel);
        midi[1] = MD_PRESSURE(el)->velocity;

        g_byte_array_append( event->data, midi, 2);

        return;
    }
    case MD_TYPE_PITCH:
    {
        root->channel[ el->device_channel].has_data = 1;
        jpmidi_time_t* time = jpmidi_get_time( root, el->element_time);
        jpmidi_event_t* event = jpmidi_event_new( el);
        jpmidi_time_add_event( time, event);
        uint8_t midi[3];

        midi[0] = 0xE0 | (0x0F & el->device_channel);
        int val = MD_PITCH(el)->pitch;
        val += 0x2000;
        midi[1] = (uint8_t)(val & 0x3F);
        midi[2] = (uint8_t)(val >> 7);

        g_byte_array_append( event->data, midi, 3);

        return;
    }
    case MD_TYPE_SYSEX:
    {
        struct sysexElement* mdSysex = MD_SYSEX(el);

        jpmidi_time_t* time = jpmidi_get_time( root, el->element_time);
        jpmidi_event_t* event = jpmidi_event_new( el);
        jpmidi_time_add_event( time, event);
        uint8_t status = 0xF0;
        
        g_byte_array_append( event->data, &status, 1);
        g_byte_array_append( event->data, mdSysex->data, mdSysex->length);
        
        return;
    }
    /* Ones that have no sequencer action */
    /*
    case MD_TYPE_TEXT:
        dumpTimeAndChannel( el);
        printf("text: type=%s; value=\"%s\"\n", MD_TEXT(el)->name, MD_TEXT(el)->text);
        return;
    case MD_TYPE_KEYSIG:
        dumpTimeAndChannel( el);
        printf("keysig: +sharps/-flats=%i, major/minor=%i\n", MD_KEYSIG(el)->key, MD_KEYSIG(el)->minor);
        return;
    case MD_TYPE_TIMESIG:
        dumpTimeAndChannel( el);
        printf("timesig: top=%i, bottom=%i, midi-clocks-per-metronome-tick=%i, num32s-per-midi-q-note=%i\n",
               MD_TIMESIG(el)->top, MD_TIMESIG(el)->bottom, MD_TIMESIG(el)->clocks, MD_TIMESIG(el)->n32pq);
        return;
    case MD_TYPE_SMPTEOFFSET:
        dumpTimeAndChannel( el);
        printf("smpte_offset\n");
        return;
    */
    default:
        // printf("WARNING: dump: not implemented yet %d\n", el->type);
        return;
    }
}

/** Returns the pathname of the MIDI file that was loaded and processed. */
char* jpmidi_get_filename( jpmidi_root_t* root)
{
    return root->filename;
}

/** Returns the first time record. */
jpmidi_time_t* jpmidi_get_time_head( jpmidi_root_t* root)
{
    return root->head;
}

/** Returns the timebase of the SMF file we loaded. */
uint16_t jpmidi_get_smf_timebase( jpmidi_root_t* root)
{
    return root->time_base;
}

/** Returns a time structure at or within one second before the
 *  specified frame. The return value may be NULL if the given frame is
 *  beyond the end of MIDI playback.
 */
jpmidi_time_t* jpmidi_lookup_entrypoint( jpmidi_root_t* root, jack_nframes_t frame)
{
    // We have an entry point every second.  Calc the frame of the previous second boundary.
    uint32_t seconds = frame / root->sample_rate;
    jack_nframes_t seek_frame = seconds * root->sample_rate;
    
    // Lookup the our time record
    return (jpmidi_time_t*)g_tree_lookup( root->data, &seek_frame);
}



/** Returns true if sending of system exclusive messages is enabled. */
int jpmidi_is_send_sysex_enabled( jpmidi_root_t* root)
{
    return root->send_sysex;
}

/** Enable/disable sending of system exclusive messages. */
void jpmidi_set_send_sysex_enabled( jpmidi_root_t* root, int enabled)
{
    root->send_sysex = enabled;
}
                                   

    
/** If a channel is being solo'ed, returns a value between 0 and
 * 15 inclusive.  Returns -1 if no channel is set for solo.
 */
int jpmidi_get_solo_channel( jpmidi_root_t* root)
{
    return root->solo_channel;
}



/** Returns true if the specified channel is currently muted. */
int jpmidi_channel_is_muted( jpmidi_root_t* root, int channel)
{
    if (channel < 0 || channel > 15) return 0;
    return root->channel[channel].muted;
}

/** Returns true if the specified channel has data. */
int jpmidi_channel_has_data( jpmidi_root_t* root, int channel)
{
    return root->channel[channel].has_data;
}


/** Returns the human readable channel number for the channel.  This is just channel+1. */
int jpmidi_channel_get_number( jpmidi_root_t* root, int channel)
{
    return root->channel[channel].number;
}

/** Returns a description based on program change found in the
 * channel.  Based on general MIDI patchset.
 */
char* jpmidi_channel_get_program( jpmidi_root_t* root, int channel)
{
    return root->channel[channel].program;
}


/** Returns the frame time of the given jpmidi_time object. */
jack_nframes_t jpmidi_time_get_frame( jpmidi_time_t* time)
{
    return time->frame;
}


/** Returns the event tick from the MIDI File (resolution specified in the file's MThd chunk). */
uint32_t jpmidi_time_get_smf_time( jpmidi_time_t* time)
{
    return time->smf_time;
}



/** Returns the next time object.  Time object are, obviously,
 * order by ascending frame time. The return value is NULL if the
 * end of MIDI data has been reached.
 */
jpmidi_time_t* jpmidi_time_get_next( jpmidi_time_t* time)
{
    return time->next_time;
}



/** Returns the number of events at the given time.  The value may
 * be zero if the time object is an entrypoint marker.
 */
int jpmidi_time_get_event_count( jpmidi_time_t* time)
{
    return time->events->len;
}



/** Returns the event object at the given index.  The value must
 * be less than the event count value returned by
 * jpmidi_time_get_event_count().
 */
jpmidi_event_t* jpmidi_time_get_event( jpmidi_time_t* time, int index)
{
    return g_array_index(time->events, jpmidi_event_t* , index);
}



/** Returns true if the event represents a system exclusive message. */
int jpmidi_event_is_sysex( jpmidi_event_t*  event)
{
    return event->data->data[0] == 0xF0;
}



/** Returns the channel to which the event is assigned. */
int jpmidi_event_get_channel( jpmidi_event_t*  event)
{
    return event->data->data[0] & 0x0F;
}



/** Returns the length in bytes of the associated message. */
int jpmidi_event_get_data_length( jpmidi_event_t*  event)
{
    return event->data->len;
}



/** Returns the event's MIDI data. */
unsigned char* jpmidi_event_get_data( jpmidi_event_t*  event)
{
    return event->data->data;
}


    
/** Returns the event's status byte with the channel bits cleared. */
unsigned char jpmidi_event_get_status( jpmidi_event_t*  event)
{
    return 0xF0 & event->data->data[0];
}
