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

#include "dump.h"
#include "jpmidi.h"

static char* gmPatchset[];

static char* controllers[128];

static int dump_inited = 0;


/** Dump events. */
jpmidi_time_t*  jpmidi_dump( jpmidi_time_t* from_time, uint32_t count, uint32_t flags)
{
    jpmidi_time_t* time = from_time;

    printf("%10s %10s %4s %4s   %s\n", "TICK", "FRAME", "CHAN", "DLEN", "DESCRIPTION");
    while (time && count-- > 0)
    {
        int i;
        for (i = 0; i < jpmidi_time_get_event_count( time); i++)
        {
            jpmidi_event_t* event = jpmidi_time_get_event( time, i);
            dump_event( time, event);
            printf("\n");
        }

        time = jpmidi_time_get_next( time);
    }
    return time;
}

void dump_event( jpmidi_time_t* time, jpmidi_event_t* event)
{
    printf("%10u %10u %4d %4d   ", jpmidi_time_get_smf_time(time), jpmidi_time_get_frame(time), jpmidi_event_get_channel( event), jpmidi_event_get_data_length(event));
    unsigned char* data = jpmidi_event_get_data( event);
    switch (jpmidi_event_get_status( event))
    {
    case 0x80: // Note off
        printf("note off: %hhu, vel: %hhu", data[1], data[2]);
        break;
    case 0x90: // Note on
        printf("note on: %hhu, vel: %hhu", data[1], data[2]);
        break;
    case 0xA0: // Key after touch
        printf("after touch: %hhu, vel: %hhu", data[1], data[2]);
        break;
    case 0xB0: // Controller
        printf("controller: %s, val: %hhu", controllers[data[1]], data[2]);
        break;
    case 0xC0: // Program change
        printf("program change: %s", dump_get_program_description(data[1]));
        break;
    case 0xD0: // Channel aftertouch
        printf("channel pressure: %hhu", data[1]);
        break;
    case 0xE0: // Pitch wheel
    {
        int val = data[1];
        val |= (data[2] << 7);
        val -= 0x2000;
        printf("pitch wheel: %d", val);
        break;
    }
    case 0xF0: // Sysex
    {
        int i;
        printf("sysex: ");
        for (i = 0; i < jpmidi_event_get_data_length( event); i++)
            printf( "%02x ", data[i]);
    }
    }
}

char* dump_get_program_description( uint8_t program)
{
    if (program > 127) return NULL;
    return gmPatchset[program];
}


static char* gmPatchset[] = {
        "Acoustic Grand",
        "Bright Acoustic",
        "Electric Grand",
        "Honky-Tonk",
        "Electric Piano 1",
        "Electric Piano 2",
        "Harpsichord",
        "Clavinet",
        "Celesta",
        "Glockenspiel",
        "Music Box",
        "Vibraphone",
        "Marimba",
        "Xylophone",
        "Tubular Bells",
        "Dulcimer",
        "Drawbar Organ",
        "Percussive Organ",
        "Rock Organ",
        "Church Organ",
        "Reed Organ",
        "Accoridan",
        "Harmonica",
        "Tango Accordian",
        "Nylon String Guitar",
        "Steel String Guitar",
        "Electric Jazz Guitar",
        "Electric Clean Guitar",
        "Electric Muted Guitar",
        "Overdriven Guitar",
        "Distortion Guitar",
        "Guitar Harmonics",
        "Acoustic Bass",
        "Electric Bass (fingered)",
        "Electric Bass (picked)",
        "Fretless Bass",
        "Slap Bass 1",
        "Slap Bass 2",
        "Synth Bass 1",
        "Synth Bass 2",
        "Violin",
        "Viola",
        "Cello",
        "Contrabass",
        "Tremolo Strings",
        "Pizzicato Strings",
        "Orchestral Strings",
        "Timpani",
        "String Ensemble 1",
        "String Ensemble 2",
        "SynthStrings 1",
        "SynthStrings 2",
        "Choir Aahs",
        "Voice Oohs",
        "Synth Voice",
        "Orchestra Hit",
        "Trumpet",
        "Trombone",
        "Tuba",
        "Muted Trumpet",
        "French Horn",
        "Brass Section",
        "SynthBrass 1",
        "SynthBrass 2",
        "Soprano Sax",
        "Alto Sax",
        "Tenor Sax",
        "Baritone Sax",
        "Oboe",
        "English Horn",
        "Bassoon",
        "Clarinet",
        "Piccolo",
        "Flute",
        "Recorder",
        "Pan Flute",
        "Blown Bottle",
        "Skakuhachi",
        "Whistle",
        "Ocarina",
        "Lead 1 (square)",
        "Lead 2 (sawtooth)",
        "Lead 3 (calliope)",
        "Lead 4 (chiff)",
        "Lead 5 (charang)",
        "Lead 6 (voice)",
        "Lead 7 (fifths)",
        "Lead 8 (bass+lead)",
        "Pad 1 (new age)",
        "Pad 2 (warm)",
        "Pad 3 (polysynth)",
        "Pad 4 (choir)",
        "Pad 5 (bowed)",
        "Pad 6 (metallic)",
        "Pad 7 (halo)",
        "Pad 8 (sweep)",
        "FX 1 (rain)",
        "FX 2 (soundtrack)",
        "FX 3 (crystal)",
        "FX 4 (atmosphere)",
        "FX 5 (brightness)",
        "FX 6 (goblins)",
        "FX 7 (echoes)",
        "FX 8 (sci-fi)",
        "Sitar",
        "Banjo",
        "Shamisen",
        "Koto",
        "Kalimba",
        "Bagpipe",
        "Fiddle",
        "Shanai",
        "Tinkle Bell",
        "Agogo",
        "Steel Drums",
        "Woodblock",
        "Taiko Drum",
        "Melodic Tom",
        "Synth Drum",
        "Reverse Cymbal",
        "Guitar Fret Noise",
        "Breath Noise",
        "Seashore",
        "Bird Tweet",
        "Telephone Ring",
        "Helicopter",
        "Applause",
        "Gunshot"
    };


void dump_init()
{

    if (dump_inited) return;
    dump_inited = 1;
    
    int i;
    for (i = 0; i < 128; i++) controllers[i] = "Undefined";
    
    controllers[0]=   "Bank Select (coarse)";
    controllers[1]=   "Modulation Wheel (coarse)";
    controllers[2]=   "Breath controller (coarse)";
    controllers[4]=   "Foot Pedal (coarse)";
    controllers[5]=   "Portamento Time (coarse)";
    controllers[6]=   "Data Entry (coarse)";
    controllers[7]=   "Volume (coarse)";
    controllers[8]=   "Balance (coarse)";
    controllers[10]=  "Pan position (coarse)";
    controllers[11]=  "Expression (coarse)";
    controllers[12]=  "Effect Control 1 (coarse)";
    controllers[13]=  "Effect Control 2 (coarse)";
    controllers[16]=  "General Purpose Slider 1";
    controllers[17]=  "General Purpose Slider 2";
    controllers[18]=  "General Purpose Slider 3";
    controllers[19]=  "General Purpose Slider 4";
    controllers[32]=  "Bank Select (fine)";
    controllers[33]=  "Modulation Wheel (fine)";
    controllers[34]=  "Breath controller (fine)";
    controllers[36]=  "Foot Pedal (fine)";
    controllers[37]=  "Portamento Time (fine)";
    controllers[38]=  "Data Entry (fine)";
    controllers[39]=  "Volume (fine)";
    controllers[40]=  "Balance (fine)";
    controllers[42]=  "Pan position (fine)";
    controllers[43]=  "Expression (fine)";
    controllers[44]=  "Effect Control 1 (fine)";
    controllers[45]=  "Effect Control 2 (fine)";
    controllers[64]=  "Hold Pedal (on/off)";
    controllers[65]=  "Portamento (on/off)";
    controllers[66]=  "Sustenuto Pedal (on/off)";
    controllers[67]=  "Soft Pedal (on/off)";
    controllers[68]=  "Legato Pedal (on/off)";
    controllers[69]=  "Hold 2 Pedal (on/off)";
    controllers[70]=  "Sound Variation";
    controllers[71]=  "Sound Timbre";
    controllers[72]=  "Sound Release Time";
    controllers[73]=  "Sound Attack Time";
    controllers[74]=  "Sound Brightness";
    controllers[75]=  "Sound Control 6";
    controllers[76]=  "Sound Control 7";
    controllers[77]=  "Sound Control 8";
    controllers[78]=  "Sound Control 9";
    controllers[79]=  "Sound Control 10";
    controllers[80]=  "General Purpose Button 1 (on/off)";
    controllers[81]=  "General Purpose Button 2 (on/off)";
    controllers[82]=  "General Purpose Button 3 (on/off)";
    controllers[83]=  "General Purpose Button 4 (on/off)";
    controllers[91]=  "Effects Level";
    controllers[92]=  "Tremulo Level";
    controllers[93]=  "Chorus Level";
    controllers[94]=  "Celeste Level";
    controllers[95]=  "Phaser Level";
    controllers[96]=  "Data Button increment";
    controllers[97]=  "Data Button decrement";
    controllers[98]=  "Non-registered Parameter (fine)";
    controllers[99]=  "Non-registered Parameter (coarse)";
    controllers[100]= "Registered Parameter (fine)";
    controllers[101]= "Registered Parameter (coarse)";
    controllers[120]= "All Sound Off";
    controllers[121]= "All Controllers Off";
    controllers[122]= "Local Keyboard (on/off)";
    controllers[123]= "All Notes Off";
    controllers[124]= "Omni Mode Off";
    controllers[125]= "Omni Mode On";
    controllers[126]= "Mono Operation";
    controllers[127]= "Poly Operation";
}
