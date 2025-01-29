#include "daisy_pod.h"
#include "daisysp.h"
#include <stdio.h>
#include <string.h>

using namespace daisy;
using namespace daisysp;

const uint8_t   MAX_KEYS_DOWN = 3;
uint8_t         numKeysDown = 0;
bool            keysUpdated = false;

const uint8_t   MAX_VOICES = 3;

DaisyPod        hw;
CpuLoadMeter    loadMeter;


static constexpr int    kNumHarmonics = 16;



MidiUsbHandler  midi;
Oscillator      osc[MAX_VOICES];
static HarmonicOscillator<kNumHarmonics> harm[MAX_VOICES];
Adsr            ampEnv[MAX_VOICES];
bool            ampEnvGate[MAX_VOICES];

NoteOnEvent     keysDown[MAX_KEYS_DOWN];



static float scale_ = 1.0f;
static float cutoff_ = 1.0f;
static float res_ = 0.0f;
static float evenOdd_ = 0.0f;

static float amplitudes_[kNumHarmonics];


static float calculateAmplitudes(float cutoff, float res, float even_odd)
{
    float a, ratio;
    float sum = 0.0f;

    cutoff      = fclamp(cutoff,    0.001f,     1.0f);
    res         = fclamp(res,       0.001f,     0.99f);
    even_odd    = fclamp(even_odd,  -1.0f,      1.0f);

    const float cut_idx = cutoff * kNumHarmonics;
    const float q = 1.0f / (2.0f * (1.0f - res));
    const float qrt = sqrt(q);
    // const float dh = cutoff / q;
    const float rh = 1 + 0.5f / q;

    for (int n = 1; n < kNumHarmonics; n++) //TODO
    {
        a = 1.0f / n;           //  Basic sawtooth

        ratio = n / cut_idx;    //  24dB/Oct LPF (4pole)
        if ( n >= cut_idx)      //  Try full cutoff, i.e. a=0 for n>cut_idx
        {
            a *= powf(0.5, log2f(ratio) * 4.f);
        }

        if ( n < cut_idx)
        {
            ratio = 1.0f + (cut_idx - n) / cut_idx;
        }
        a *= qrt * powf( 0.5, (ratio - 1.0f) / (rh - 1.0f) );

        if ( n % 2 == 0)         //  Even/odd balance
        {
            a *= 1.0f - fclamp( even_odd, 0.0f, 1.0f);
        }
        else 
        {
            a *= 1.0f + fclamp( even_odd, -1.0f, 0.0f);
        }

        amplitudes_[n-1] = a;
        sum += a;
    }

    if (sum < 1.0f) return 1.0f;
    return 1.0f / sum;
}


void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    loadMeter.OnBlockStart();

    for(size_t i = 0; i < size; i++)
    {
        float sig = 0;
        for(int i = 0; i < MAX_VOICES; i++)
        {
//            sig += ampEnv[i].Process(ampEnvGate[i]) * osc[i].Process();
//            sig += ampEnv[i].Process(ampEnvGate[i]) * harm[i].Process();
            sig += ampEnv[i].Process(ampEnvGate[i]) * harm[i].Process() * 0.5f * scale_;
        }
        out[0][i] = out[1][i] = sig;
    }

    loadMeter.OnBlockEnd();
}


void ProcessKeyUp(NoteOffEvent noteToRemove)
{
//Lol
//osc[0].SetAmp(0);
ampEnvGate[0]=false;

    uint8_t i=0;
    while( i < numKeysDown ) {
        if( keysDown[i].note == noteToRemove.note ) {
            while( i < numKeysDown - 1 ) {
                keysDown[i].note = keysDown[i+1].note;
                i++;
            }
            keysDown[i].note = 0;
            numKeysDown--;
            keysUpdated = true;
            return;
        }
        i++;
    }
}


void ProcessKeyDown(NoteOnEvent noteToAdd)
{
//Lol
//osc[0].SetAmp(0.1f);
ampEnv[0].Retrigger(false);
ampEnvGate[0]=true;

    for( uint8_t i=0; i<MAX_KEYS_DOWN; i++) {       // Incrementing index for examining entries

        if( keysDown[i].note == 0 ) {   //  Empty slot at end of list
            if( numKeysDown == MAX_KEYS_DOWN ) {
                NoteOffEvent noteToRemove;
                noteToRemove.note = keysDown[MAX_KEYS_DOWN-1].note;
                ProcessKeyUp(noteToRemove);
            }
            keysDown[i] = noteToAdd;
            numKeysDown++;
            keysUpdated = true;
            return;
        }

        if( noteToAdd.note < keysDown[i].note ) {   //  New note is lower than indexed note
            continue;                       //  Move on to next index
        }
        else {                              //  New note is higher than indexed note
            if( numKeysDown == MAX_KEYS_DOWN ) {
                NoteOffEvent noteToRemove;
                noteToRemove.note = keysDown[MAX_KEYS_DOWN-1].note;
                ProcessKeyUp(noteToRemove);
            }
            uint8_t d = numKeysDown;    // Decrementing counter for shifting entries
            while ( d > i )
            {
                keysDown[d] = keysDown[d-1];
                d--;
            }
            keysDown[i] = noteToAdd;
            numKeysDown++;  
            keysUpdated = true;
            return;
        }
        //TODO  Could get to here if list is full of notes higher than new note 
        //TODO  Could get to here if new note is same as one already playing but this should never happen
    }
}


void ReportKeysDown(void)
{
    hw.seed.Print( "    numKeysDown: %d ", numKeysDown );
    hw.seed.Print( "keysDown: " );
    for( uint8_t i=0; i<MAX_KEYS_DOWN; ++i) {
        hw.seed.Print( "%03d, ", keysDown[i].note );
    }

//    maxLoad = loadMeter.GetMaxCpuLoad();
//    hw.seed.Print(" Processing Load Min:" FLT_FMT3, FLT_VAR3(loadMeter.GetMinCpuLoad() * 100.0f));
//    hw.seed.Print(" Avg:" FLT_FMT3, FLT_VAR3(loadMeter.GetAvgCpuLoad() * 100.0f));
    hw.seed.Print(" ProcMax:" FLT_FMT3, FLT_VAR3(loadMeter.GetMaxCpuLoad() * 100.0f));
    loadMeter.Reset();

    hw.seed.Print(" amp[ ");
    for( uint8_t i=0; i<kNumHarmonics; i++) {
        hw.seed.Print("%d",i);
        hw.seed.Print(":" FLT_FMT3, FLT_VAR3(amplitudes_[i]));
        hw.seed.Print(" ");
    }
    hw.seed.Print("] scale_:" FLT_FMT3, FLT_VAR3(scale_));

    hw.seed.Print( "\r\n" );
}

void MapKeysToNotes(void)
{
// memcpy keysDown to wantedNotes[]
}
    
    
    
void UpdateVoices(void)
{
// Make use of FIFO.h, Stack.h, 

//  for each voice
//      if no longer in wantedNotes[]
//          envelope to release
//      Prioritise

//  for each wantedNote[]
//      if already voiced
//          done
//      else
//          assign to lowest priority voice
//          setFreq and 
//            ampEnv[i].Retrigger(false);

if(keysDown[0].note != 0){
osc[0].SetFreq(mtof((float)keysDown[0].note));
harm[0].SetFreq(mtof((float)keysDown[0].note));
}

}


void HandleMidiMessage(MidiEvent m)
{
    hw.seed.Print("HandleMidiMessage(%3d,%3d,%3d,%3d): ",m.type,m.channel,m.data[0],m.data[1]);

    switch(m.type)
    {
        case NoteOff:
        {
//            hw.seed.Print("MIDI NoteOff:            ");
            ProcessKeyUp(m.AsNoteOff());
            break;
        }

        case NoteOn:
        {
//            hw.seed.Print("MIDI NoteOn:             ");
            NoteOnEvent noteToAdd = m.AsNoteOn();
            if(noteToAdd.velocity == 0) {
                ProcessKeyUp(m.AsNoteOff());
            }
            else {
                ProcessKeyDown(noteToAdd);
            }
            break;
        }

        case ControlChange:
        {
//            hw.seed.Print("MIDI ControlChangeEvent: ");
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

        }

        default:
//            hw.seed.Print("MIDI other event:        ");
            break;
    }

    ReportKeysDown();

}


void InitSynth(float sampleRate)
{

//lol
//    calculateAmplitudes( 1.0f, 0.0f, 0.0f);
hw.seed.PrintLine("InitSynth()");
scale_ = calculateAmplitudes( 1.0f, 0.0f, 0.0f);
ReportKeysDown();

    for(int i = 0; i < MAX_VOICES; i++)
    {
        osc[i].Init(sampleRate);
        osc[i].SetAmp(1.0f/MAX_VOICES);
        osc[i].SetWaveform(Oscillator::WAVE_SIN);

        harm[i].Init(sampleRate);
        harm[i].SetFirstHarmIdx(1);
//            void SetAmplitudes(const float* amplitudes)
        harm[i].SetAmplitudes(amplitudes_);

//    void SetSingleAmp(const float amp, int idx)
//        harm[i].SetSingleAmp(0.5f/MAX_VOICES, 0);
//        harm[i].SetSingleAmp(0.5f/MAX_VOICES, 3);



        ampEnv[i].Init(sampleRate);
        ampEnv[i].SetAttackTime(0.25);
        ampEnv[i].SetDecayTime(0.5);
        ampEnv[i].SetSustainLevel(0.5);
        ampEnv[i].SetReleaseTime(1);
        ampEnvGate[i] = false;
        
    }

}



int main(void)
{
//    float cutoff    = fmap( normalAnalogRead(kCutoffControlPin), 0.001f, 1.0f, Mapping::LOG);
    float cutoff    = 1.0f;
//    float res       = normalAnalogRead(kCutoffControlPin);
    float res       = 0.0f;
//    float evenodd   = fmap( normalAnalogRead(kEvenOddControlPin), -1.0f, 1.0f);
    float evenOdd   = 0.0f;


    hw.Init();
    hw.seed.StartLog(true);              // Start the log, and wait for connection
    hw.seed.PrintLine("Hello World!");   // Print "Hello World" to the Serial Monitor
    System::Delay(250);

    loadMeter.Init(hw.AudioSampleRate(), hw.AudioBlockSize());

    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbTransport::Config::EXTERNAL;
    midi.Init(midi_cfg);

    InitSynth(hw.AudioSampleRate());

    hw.StartAudio(AudioCallback);		//	And start the audio callback
    midi.StartReceive();

    while(1){

//todo Move this to somewhere it's only done when needed
// If reading from ADC, might neeed to filter, but from MIDI CC probably use DaisySP line()
//        fonepole( cutoff_, cutoff, 0.01f ); //  0.01 of Fs = 480Hz
//        fonepole( res_, res, 0.01f ); //  0.01 of Fs = 480Hz
        cutoff_ = cutoff;
        res_ = res;
        evenOdd_ = evenOdd;
        scale_ = calculateAmplitudes(cutoff_, res_, evenOdd_);





//todo Can MIDI be moved to a callback?
        midi.Listen();					//	Listen to MIDI for new changes
        while(midi.HasEvents())			//	When there are messages waiting in the queue...
        {
            HandleMidiMessage(midi.PopEvent());
            if( keysUpdated ){
                MapKeysToNotes();
                UpdateVoices();
                keysUpdated = false;
            }
        }




    }
}
