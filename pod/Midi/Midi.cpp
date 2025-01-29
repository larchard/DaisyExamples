#include "daisy_pod.h"
#include "daisysp.h"
#include <stdio.h>
#include <string.h>

using namespace daisy;
using namespace daisysp;

char        buff[512];


DaisyPod   hw;
Oscillator osc;
Svf        filt;

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    float sig;
    for(size_t i = 0; i < size; i += 2)
    {
        sig = osc.Process();
        filt.Process(sig);
        out[i] = out[i + 1] = filt.Low();
    }
}



// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    sprintf(buff, "HandleMidiMessage(%d,%d,%d,%d):\r\n",m.type,m.channel,m.data[0],m.data[1]);            hw.seed.usb_handle.TransmitInternal((uint8_t *)buff, strlen(buff));
    switch(m.type)
    {
        case NoteOn:
        {
            /*lol*/ sprintf(buff, "MIDI NoteOnEvent:\r\n");            hw.seed.usb_handle.TransmitInternal((uint8_t *)buff, strlen(buff));
            NoteOnEvent p = m.AsNoteOn();
            sprintf(buff,
                    "Note Received:\t%d\t%d\t%d\r\n",
                    m.channel,
                    m.data[0],
                    m.data[1]);
            hw.seed.usb_handle.TransmitInternal((uint8_t *)buff, strlen(buff));
            // This is to avoid Max/MSP Note outs for now..
            if(m.data[1] != 0)
            {
                p = m.AsNoteOn();
                osc.SetFreq(mtof(p.note));
                osc.SetAmp((p.velocity / 127.0f));
            }
        }
        break;
        case ControlChange:
        {
            /*lol*/ sprintf(buff, "MIDI ControlChangeEvent:\r\n");            hw.seed.usb_handle.TransmitInternal((uint8_t *)buff, strlen(buff));
            ControlChangeEvent p = m.AsControlChange();
            switch(p.control_number)
            {
                case 1:
                    // CC 1 for cutoff.
                    filt.SetFreq(mtof((float)p.value));
                    break;
                case 2:
                    // CC 2 for res.
                    filt.SetRes(((float)p.value / 127.0f));
                    break;
                default: break;
            }
            break;
        }
        break;
        default:
        /*lol*/ sprintf(buff, "MIDI other event:\r\n");            hw.seed.usb_handle.TransmitInternal((uint8_t *)buff, strlen(buff));
        break;
    }
}


// Main -- Init, and Midi Handling
int main(void)
{
//    bool led_state;
//    led_state = true;


    // Init
    float samplerate;
    
    hw.Init();
    hw.SetAudioBlockSize(4);
    hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);
    System::Delay(250);

    // Synthesis
    samplerate = hw.AudioSampleRate();
    osc.Init(samplerate);
    osc.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
    filt.Init(samplerate);

    // Start stuff.
    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    hw.midi.StartReceive();

/*lol*/ sprintf(buff, "hw.midi.StartReceive():\r\n");    hw.seed.usb_handle.TransmitInternal((uint8_t *)buff, strlen(buff));




    for(;;)
    {
        hw.midi.Listen();
        // Handle MIDI Events
        while(hw.midi.HasEvents())
        {
            hw.led1.SetRed(1);  hw.UpdateLeds();
///*lol*/ sprintf(buff, "MIDI event:\r\n");            hw.seed.usb_handle.TransmitInternal((uint8_t *)buff, strlen(buff));
            HandleMidiMessage(hw.midi.PopEvent());
            hw.led1.SetRed(0);  hw.UpdateLeds();
        }

//        hw.seed.SetLed(led_state);
//        led_state = !led_state;

//        hw.led1.Set(r, g, b); hw.UpdateLeds();

        hw.seed.SetLed(1);
//        hw.led1.Set(0.1, 0, 0); 
//        hw.led2.Set(0, 0.5, 0); 
        hw.UpdateLeds();
        System::Delay(100);
        hw.seed.SetLed(0); hw.led1.Set(0, 0, 0); hw.led2.Set(0, 0, 0); hw.UpdateLeds();
        System::Delay(100);
        sprintf(buff, ".");            hw.seed.usb_handle.TransmitInternal((uint8_t *)buff, strlen(buff));
    }
}
