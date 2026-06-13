#ifndef SONG_H
#define SONG_H

#include <stdbool.h>

#define SONG_MAX_NOTES 8192
#define SONG_PPQ 960
#define SONG_MAX_ACTIVE_MIDI_NOTES 128

typedef struct Note {
    int midi;              // 0..127
    int velocity;          // 1..127

    // Raw recording time, in seconds from the start of the take.
    // BPM/grid changes rebuild the musical ticks from these raw values.
    double rawStartSec;
    double rawEndSec;

    // Quantized/editable music position.
    int startTick;
    int lenTick;

    bool selected;
} Note;

typedef struct Song {
    Note notes[SONG_MAX_NOTES];
    int noteCount;

    double bpm;
    int gridDenom;         // 16 means sixteenth-note snap, 32 means 32nd-note snap, etc.
    int timeSigTop;
    int timeSigBottom;

    bool dirty;
} Song;

void Song_Init(Song* song);
void Song_Clear(Song* song);
int  Song_GridTicks(const Song* song);
int  Song_TicksPerWhole(void);
int  Song_TicksPerBeat(const Song* song);
int  Song_SecondsToTicks(const Song* song, double sec);
double Song_TicksToSeconds(const Song* song, int ticks);
int  Song_QuantizeTick(const Song* song, int tick);
void Song_RequantizeNoteFromRaw(Song* song, int index);
void Song_RequantizeAllFromRaw(Song* song);
int  Song_AddRawNote(Song* song, int midi, int velocity, double startSec, double endSec);
void Song_RemoveNote(Song* song, int index);
void Song_Sort(Song* song);
int  Song_SelectedIndex(const Song* song);
void Song_SelectOnly(Song* song, int index);
void Song_DeselectAll(Song* song);
void Song_MoveSelectedPitch(Song* song, int semis);
void Song_MoveSelectedStart(Song* song, int ticks);
void Song_ResizeSelected(Song* song, int ticks);
void Song_CycleGrid(Song* song);
void Song_SetBpm(Song* song, double bpm, bool requantizeRaw);
bool Song_SaveProject(const Song* song, const char* path);
bool Song_LoadProject(Song* song, const char* path);

#endif
