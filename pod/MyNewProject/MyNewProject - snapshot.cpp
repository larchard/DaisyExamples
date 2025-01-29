#include "daisy_pod.h"
#include "daisysp.h"
#include <stdio.h>
#include <string.h>

using namespace daisy;
using namespace daisysp;

const uint8_t MAX_KEYS_DOWN = 3;
uint8_t numKeysDown = 0;

DaisyPod       hw;
MidiUsbHandler midi;
Oscillator     osc[MAX_KEYS_DOWN];

//NoteOnEvent keysDown[MAX_KEYS_DOWN];
uint8_t keysDown[MAX_KEYS_DOWN];


void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        float sig = 0;
        for(int i = 0; i < MAX_KEYS_DOWN; i++)
        {
            sig += osc[i].Process();
        }

        out[0][i] = out[1][i] = sig;

    }
}


//void ProcessNoteOff(NoteOffEvent noteToRemove)
void ProcessNoteOff(uint8_t noteToRemove)
{
    uint8_t i=0;
    while( i < numKeysDown ) {
//        if( keysDown[i].note == noteToRemove.note ) {
        if( keysDown[i] == noteToRemove ) {
            while( i < numKeysDown - 1 ) {
//                keysDown[i].note = keysDown[i+1].note;
                keysDown[i] = keysDown[i+1];
                i++;
            }
//            keysDown[i].note = 0;
            keysDown[i] = 0;
            numKeysDown--;
            osc[i].SetAmp(0);
            return;
        }
        i++;
    }
}


//void ProcessNoteOn(NoteOnEvent noteToAdd)
void ProcessNoteOn(uint8_t noteToAdd)   
{
//TODO  Could this be more elegant? with for( uint8_t i=0; i<=numKeysDown; i++)
//TODO  Could this be more elegant? by avoiding if( keysDown[i] == 0 ) as special case
    for( uint8_t i=0; i<MAX_KEYS_DOWN; i++) {       // Incrementing index for examining entries
//        if( keysDown[i].note == 0 ) {   //  Empty slot at end of list
//            keysDown[i].note = noteToAdd.note;
        if( keysDown[i] == 0 ) {   //  Empty slot at end of list
            if( numKeysDown == MAX_KEYS_DOWN ) {
                ProcessNoteOff(keysDown[MAX_KEYS_DOWN-1]);
            }
            keysDown[i] = noteToAdd;
            numKeysDown++;
            osc[i].SetFreq(mtof(keysDown[i]));
            osc[i].SetAmp(0.5f);                 //TODO Use aftertouch value
            return;
        }

//        if( noteToAdd.note < keysDown[i].note ) {   //  New note is lower than indexed note
        if( noteToAdd < keysDown[i] ) {   //  New note is lower than indexed note
            continue;                       //  Move on to next index
        }
        else {                              //  New note is higher than indexed note
            if( numKeysDown == MAX_KEYS_DOWN ) {
                ProcessNoteOff(keysDown[MAX_KEYS_DOWN-1]);
            }
            uint8_t d = numKeysDown;    // Decrementing counter for shifting entries
            while ( d > i )
            {
                keysDown[d] = keysDown[d-1];
                d--;
            }
            keysDown[i] = noteToAdd;
            numKeysDown++;  
            osc[i].SetFreq(mtof(keysDown[i]));
            osc[i].SetAmp(0.5f);                 //TODO Use aftertouch value
            return;
        }
        //TODO  Could get to here if list is full of notes higher than new note 
        //TODO  Could get to here if new note is same as one already playing
    }
}


void ReportKeysDown(void)
{
    hw.seed.Print( "    numKeysDown: %d ", numKeysDown );
    hw.seed.Print( "keysDown: " );
    for( uint8_t i=0; i<MAX_KEYS_DOWN; ++i) {
//        hw.seed.Print( "%d, ", keysDown[i].note );
        hw.seed.Print( "%03d ", keysDown[i] );
    }
    hw.seed.Print( "\r\n" );
}


void HandleMidiMessage(MidiEvent m)
{
    hw.seed.Print("HandleMidiMessage(%3d,%3d,%3d,%3d): ",m.type,m.channel,m.data[0],m.data[1]);

    switch(m.type)
    {
        case NoteOff:
        {
//            ProcessNoteOff(m.AsNoteOff());
            ProcessNoteOff(m.AsNoteOff().note);

//            NoteOffEvent p = m.AsNoteOff();
//            hw.seed.Print("NoteOff: Ch=%d, Note=%d, Vel=%d\r\n", p.channel, p.note, p.velocity);
            break;
        }

        case NoteOn:
        {
            NoteOnEvent noteToAdd = m.AsNoteOn();
            if(noteToAdd.velocity == 0) {
//                ProcessNoteOff(m.AsNoteOff());
                ProcessNoteOff(m.AsNoteOff().note);
            }
            else {
//                ProcessNoteOn(noteToAdd);
                ProcessNoteOn(noteToAdd.note);
            }
            break;
        }

        case ControlChange:
        {
            ControlChangeEvent p = m.AsControlChange();
            switch(p.control_number)
            {
                case 1:
                    // CC 1 for cutoff.
 //                   filt.SetFreq(mtof((float)p.value));
                    break;
                case 2:
                    // CC 2 for res.
 //                   filt.SetRes(((float)p.value / 127.0f));
                    break;
                default: 
                    break;
            }
            break;

            hw.seed.Print("MIDI ControlChangeEvent:\r\n");
        }

        default:
            hw.seed.Print("MIDI other event:\r\n");
            break;
    }

}


void InitSynth(float samplerate)
{
    for(int i = 0; i < MAX_KEYS_DOWN; i++)
    {
        osc[i].Init(samplerate);
        osc[i].SetAmp(0);
        osc[i].SetWaveform(Oscillator::WAVE_SIN);
    }
}


int main(void)
{
    bool led_state;
    led_state = true;

    hw.Init();
    hw.seed.StartLog(true);              // Start the log, and wait for connection
    hw.seed.PrintLine("Hello World!");   // Print "Hello World" to the Serial Monitor
    System::Delay(250);


    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbTransport::Config::EXTERNAL;
    midi.Init(midi_cfg);

    InitSynth(hw.AudioSampleRate());

    hw.StartAudio(AudioCallback);		//	And start the audio callback
    midi.StartReceive();

    while(1){
        midi.Listen();					//	Listen to MIDI for new changes
        while(midi.HasEvents())			//	When there are messages waiting in the queue...
        {
            HandleMidiMessage(midi.PopEvent());
            ReportKeysDown();
        }
    }
}
