#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

#include <stdbool.h>

typedef struct MidiEvent {
    unsigned int message;
    double timeSec;
} MidiEvent;

typedef struct MidiInput MidiInput;

void MidiInput_Init(MidiInput* midi);
bool MidiInput_OpenFirst(MidiInput* midi);
void MidiInput_Close(MidiInput* midi);
bool MidiInput_IsOpen(const MidiInput* midi);
int  MidiInput_DeviceCount(void);
const char* MidiInput_DeviceName(const MidiInput* midi);
const char* MidiInput_StatusText(const MidiInput* midi);
bool MidiInput_PollEvent(MidiInput* midi, MidiEvent* outEvent);
double MidiInput_NowSec(void);

struct MidiInput {
    void* handle;
    char deviceName[128];
    char statusText[256];
    int deviceId;
    bool open;

#ifdef _WIN32
    void* lockPtr;
#endif

    MidiEvent queue[2048];
    int readIndex;
    int writeIndex;
    int droppedEvents;
};

#endif
