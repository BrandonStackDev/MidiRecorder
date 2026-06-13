#include "midi_input.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <mmsystem.h>

    typedef struct MidiLockBox {
        CRITICAL_SECTION cs;
    } MidiLockBox;

    static void MidiLock(MidiInput* midi)
    {
        if (!midi || !midi->lockPtr) return;
        EnterCriticalSection(&((MidiLockBox*)midi->lockPtr)->cs);
    }

    static void MidiUnlock(MidiInput* midi)
    {
        if (!midi || !midi->lockPtr) return;
        LeaveCriticalSection(&((MidiLockBox*)midi->lockPtr)->cs);
    }

    static void CALLBACK MidiInput_Callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
    {
        (void)hMidiIn;
        (void)dwParam2;

        if (wMsg != MIM_DATA) return;

        MidiInput* midi = (MidiInput*)dwInstance;
        if (!midi) return;

        MidiLock(midi);

        int next = (midi->writeIndex + 1) % (int)(sizeof(midi->queue) / sizeof(midi->queue[0]));
        if (next == midi->readIndex) {
            midi->droppedEvents++;
        }
        else {
            midi->queue[midi->writeIndex].message = (unsigned int)dwParam1;
            midi->queue[midi->writeIndex].timeSec = MidiInput_NowSec();
            midi->writeIndex = next;
        }

        MidiUnlock(midi);
    }
#endif

double MidiInput_NowSec(void)
{
#ifdef _WIN32
    return (double)timeGetTime() / 1000.0;
#else
    return 0.0;
#endif
}

void MidiInput_Init(MidiInput* midi)
{
    if (!midi) return;
    memset(midi, 0, sizeof(*midi));
    midi->deviceId = -1;
    snprintf(midi->deviceName, sizeof(midi->deviceName), "none");
    snprintf(midi->statusText, sizeof(midi->statusText), "MIDI not opened yet");

#ifdef _WIN32
    MidiLockBox* box = (MidiLockBox*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MidiLockBox));
    if (box) {
        InitializeCriticalSection(&box->cs);
        midi->lockPtr = box;
    }
#endif
}

int MidiInput_DeviceCount(void)
{
#ifdef _WIN32
    return (int)midiInGetNumDevs();
#else
    return 0;
#endif
}

bool MidiInput_OpenFirst(MidiInput* midi)
{
    if (!midi) return false;
    if (midi->open) return true;

#ifdef _WIN32
    UINT count = midiInGetNumDevs();
    if (count <= 0) {
        snprintf(midi->statusText, sizeof(midi->statusText), "No MIDI input devices found. Plug in keyboard, restart app.");
        return false;
    }

    UINT deviceId = 0;
    MIDIINCAPSA caps;
    memset(&caps, 0, sizeof(caps));
    MMRESULT capsResult = midiInGetDevCapsA(deviceId, &caps, sizeof(caps));
    if (capsResult == MMSYSERR_NOERROR) {
        snprintf(midi->deviceName, sizeof(midi->deviceName), "%s", caps.szPname);
    }
    else {
        snprintf(midi->deviceName, sizeof(midi->deviceName), "MIDI input 0");
    }

    HMIDIIN handle = NULL;
    MMRESULT result = midiInOpen(&handle, deviceId, (DWORD_PTR)MidiInput_Callback, (DWORD_PTR)midi, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        snprintf(midi->statusText, sizeof(midi->statusText), "midiInOpen failed: %u", (unsigned int)result);
        return false;
    }

    result = midiInStart(handle);
    if (result != MMSYSERR_NOERROR) {
        midiInClose(handle);
        snprintf(midi->statusText, sizeof(midi->statusText), "midiInStart failed: %u", (unsigned int)result);
        return false;
    }

    midi->handle = handle;
    midi->deviceId = (int)deviceId;
    midi->open = true;
    midi->readIndex = 0;
    midi->writeIndex = 0;
    midi->droppedEvents = 0;
    snprintf(midi->statusText, sizeof(midi->statusText), "MIDI open: %s", midi->deviceName);
    return true;
#else
    snprintf(midi->statusText, sizeof(midi->statusText), "MIDI input is only implemented for Windows winmm in this starter.");
    return false;
#endif
}

void MidiInput_Close(MidiInput* midi)
{
    if (!midi) return;

#ifdef _WIN32
    if (midi->open && midi->handle) {
        HMIDIIN handle = (HMIDIIN)midi->handle;
        midiInStop(handle);
        midiInReset(handle);
        midiInClose(handle);
    }

    midi->handle = NULL;
    midi->open = false;
    midi->deviceId = -1;
    snprintf(midi->statusText, sizeof(midi->statusText), "MIDI closed");

    if (midi->lockPtr) {
        MidiLockBox* box = (MidiLockBox*)midi->lockPtr;
        DeleteCriticalSection(&box->cs);
        HeapFree(GetProcessHeap(), 0, box);
        midi->lockPtr = NULL;
    }
#else
    midi->open = false;
#endif
}

bool MidiInput_IsOpen(const MidiInput* midi)
{
    return midi && midi->open;
}

const char* MidiInput_DeviceName(const MidiInput* midi)
{
    if (!midi) return "none";
    return midi->deviceName;
}

const char* MidiInput_StatusText(const MidiInput* midi)
{
    if (!midi) return "no midi";
    return midi->statusText;
}

bool MidiInput_PollEvent(MidiInput* midi, MidiEvent* outEvent)
{
    if (!midi || !outEvent) return false;

#ifdef _WIN32
    MidiLock(midi);
#endif

    bool have = (midi->readIndex != midi->writeIndex);
    if (have) {
        *outEvent = midi->queue[midi->readIndex];
        midi->readIndex = (midi->readIndex + 1) % (int)(sizeof(midi->queue) / sizeof(midi->queue[0]));
    }

#ifdef _WIN32
    MidiUnlock(midi);
#endif

    return have;
}
