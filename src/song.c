#include "song.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int ClampIntLocal(int v, int mn, int mx)
{
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

static double ClampDoubleLocal(double v, double mn, double mx)
{
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

void Song_Init(Song* song)
{
    if (!song) return;
    memset(song, 0, sizeof(*song));
    song->bpm = 120.0;
    song->gridDenom = 16;
    song->timeSigTop = 4;
    song->timeSigBottom = 4;
    song->dirty = false;
}

void Song_Clear(Song* song)
{
    if (!song) return;
    double bpm = song->bpm;
    int gridDenom = song->gridDenom;
    int top = song->timeSigTop;
    int bottom = song->timeSigBottom;
    memset(song, 0, sizeof(*song));
    song->bpm = bpm;
    song->gridDenom = gridDenom;
    song->timeSigTop = top;
    song->timeSigBottom = bottom;
    song->dirty = true;
}

int Song_TicksPerWhole(void)
{
    return SONG_PPQ * 4;
}

int Song_TicksPerBeat(const Song* song)
{
    (void)song;
    return SONG_PPQ;
}

int Song_GridTicks(const Song* song)
{
    int denom = song ? song->gridDenom : 16;
    if (denom <= 0) denom = 16;
    int ticks = Song_TicksPerWhole() / denom;
    if (ticks < 1) ticks = 1;
    return ticks;
}

int Song_SecondsToTicks(const Song* song, double sec)
{
    double bpm = song ? song->bpm : 120.0;
    double beats = sec * bpm / 60.0;
    double ticks = beats * (double)SONG_PPQ;
    if (ticks < 0.0) ticks = 0.0;
    return (int)llround(ticks);
}

double Song_TicksToSeconds(const Song* song, int ticks)
{
    double bpm = song ? song->bpm : 120.0;
    if (bpm < 1.0) bpm = 1.0;
    return ((double)ticks / (double)SONG_PPQ) * (60.0 / bpm);
}

int Song_QuantizeTick(const Song* song, int tick)
{
    int grid = Song_GridTicks(song);
    if (grid <= 0) return tick;
    if (tick < 0) tick = 0;
    return ((tick + grid / 2) / grid) * grid;
}

void Song_RequantizeNoteFromRaw(Song* song, int index)
{
    if (!song) return;
    if (index < 0 || index >= song->noteCount) return;

    Note* n = &song->notes[index];
    int start = Song_QuantizeTick(song, Song_SecondsToTicks(song, n->rawStartSec));
    int end = Song_QuantizeTick(song, Song_SecondsToTicks(song, n->rawEndSec));
    int grid = Song_GridTicks(song);

    if (end <= start) end = start + grid;

    n->startTick = start;
    n->lenTick = end - start;
    if (n->lenTick < grid) n->lenTick = grid;
}

void Song_RequantizeAllFromRaw(Song* song)
{
    if (!song) return;
    for (int i = 0; i < song->noteCount; i++) {
        Song_RequantizeNoteFromRaw(song, i);
    }
    Song_Sort(song);
    song->dirty = true;
}

int Song_AddRawNote(Song* song, int midi, int velocity, double startSec, double endSec)
{
    if (!song) return -1;
    if (song->noteCount >= SONG_MAX_NOTES) return -1;

    if (endSec < startSec) {
        double t = startSec;
        startSec = endSec;
        endSec = t;
    }

    if ((endSec - startSec) < 0.010) {
        endSec = startSec + 0.050;
    }

    int idx = song->noteCount++;
    Note* n = &song->notes[idx];
    memset(n, 0, sizeof(*n));
    n->midi = ClampIntLocal(midi, 0, 127);
    n->velocity = ClampIntLocal(velocity, 1, 127);
    n->rawStartSec = startSec;
    n->rawEndSec = endSec;
    n->selected = false;

    Song_RequantizeNoteFromRaw(song, idx);
    song->dirty = true;
    return idx;
}

void Song_RemoveNote(Song* song, int index)
{
    if (!song) return;
    if (index < 0 || index >= song->noteCount) return;

    for (int i = index; i < song->noteCount - 1; i++) {
        song->notes[i] = song->notes[i + 1];
    }
    song->noteCount--;
    song->dirty = true;
}

static int NoteCompare(const void* a, const void* b)
{
    const Note* na = (const Note*)a;
    const Note* nb = (const Note*)b;
    if (na->startTick != nb->startTick) return na->startTick - nb->startTick;
    if (na->midi != nb->midi) return na->midi - nb->midi;
    return na->lenTick - nb->lenTick;
}

void Song_Sort(Song* song)
{
    if (!song || song->noteCount <= 1) return;
    qsort(song->notes, (size_t)song->noteCount, sizeof(Note), NoteCompare);
}

int Song_SelectedIndex(const Song* song)
{
    if (!song) return -1;
    for (int i = 0; i < song->noteCount; i++) {
        if (song->notes[i].selected) return i;
    }
    return -1;
}

void Song_SelectOnly(Song* song, int index)
{
    if (!song) return;
    for (int i = 0; i < song->noteCount; i++) {
        song->notes[i].selected = (i == index);
    }
}

void Song_DeselectAll(Song* song)
{
    if (!song) return;
    for (int i = 0; i < song->noteCount; i++) song->notes[i].selected = false;
}

static void UpdateRawFromTicks(Song* song, Note* n)
{
    if (!song || !n) return;
    n->rawStartSec = Song_TicksToSeconds(song, n->startTick);
    n->rawEndSec = Song_TicksToSeconds(song, n->startTick + n->lenTick);
}

void Song_MoveSelectedPitch(Song* song, int semis)
{
    int idx = Song_SelectedIndex(song);
    if (idx < 0) return;
    Note* n = &song->notes[idx];
    n->midi = ClampIntLocal(n->midi + semis, 0, 127);
    song->dirty = true;
}

void Song_MoveSelectedStart(Song* song, int ticks)
{
    int idx = Song_SelectedIndex(song);
    if (idx < 0) return;
    Note* n = &song->notes[idx];
    n->startTick += ticks;
    if (n->startTick < 0) n->startTick = 0;
    UpdateRawFromTicks(song, n);
    song->dirty = true;
    Song_Sort(song);
}

void Song_ResizeSelected(Song* song, int ticks)
{
    int idx = Song_SelectedIndex(song);
    if (idx < 0) return;
    Note* n = &song->notes[idx];
    int grid = Song_GridTicks(song);
    n->lenTick += ticks;
    if (n->lenTick < grid) n->lenTick = grid;
    UpdateRawFromTicks(song, n);
    song->dirty = true;
}

void Song_CycleGrid(Song* song)
{
    if (!song) return;
    const int values[] = { 8, 16, 32, 64, 128 };
    const int count = (int)(sizeof(values) / sizeof(values[0]));

    int next = values[0];
    for (int i = 0; i < count; i++) {
        if (song->gridDenom == values[i]) {
            next = values[(i + 1) % count];
            break;
        }
    }

    song->gridDenom = next;
    Song_RequantizeAllFromRaw(song);
}

void Song_SetBpm(Song* song, double bpm, bool requantizeRaw)
{
    if (!song) return;
    song->bpm = ClampDoubleLocal(bpm, 20.0, 300.0);
    if (requantizeRaw) Song_RequantizeAllFromRaw(song);
    song->dirty = true;
}

bool Song_SaveProject(const Song* song, const char* path)
{
    if (!song || !path) return false;
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    fprintf(f, "MIDI_SHEET_RAYLIB_PROJECT 1\n");
    fprintf(f, "bpm %.10f\n", song->bpm);
    fprintf(f, "grid %d\n", song->gridDenom);
    fprintf(f, "time %d %d\n", song->timeSigTop, song->timeSigBottom);
    fprintf(f, "notes %d\n", song->noteCount);
    for (int i = 0; i < song->noteCount; i++) {
        const Note* n = &song->notes[i];
        fprintf(f, "note %d %d %.10f %.10f %d %d\n",
            n->midi, n->velocity, n->rawStartSec, n->rawEndSec, n->startTick, n->lenTick);
    }

    fclose(f);
    return true;
}

bool Song_LoadProject(Song* song, const char* path)
{
    if (!song || !path) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    Song loaded;
    Song_Init(&loaded);

    char word[64] = { 0 };
    int version = 0;
    if (fscanf(f, "%63s %d", word, &version) != 2 || strcmp(word, "MIDI_SHEET_RAYLIB_PROJECT") != 0) {
        fclose(f);
        return false;
    }

    int expectedNotes = 0;
    while (fscanf(f, "%63s", word) == 1) {
        if (strcmp(word, "bpm") == 0) {
            fscanf(f, "%lf", &loaded.bpm);
        }
        else if (strcmp(word, "grid") == 0) {
            fscanf(f, "%d", &loaded.gridDenom);
        }
        else if (strcmp(word, "time") == 0) {
            fscanf(f, "%d %d", &loaded.timeSigTop, &loaded.timeSigBottom);
        }
        else if (strcmp(word, "notes") == 0) {
            fscanf(f, "%d", &expectedNotes);
            if (expectedNotes < 0) expectedNotes = 0;
            if (expectedNotes > SONG_MAX_NOTES) expectedNotes = SONG_MAX_NOTES;
        }
        else if (strcmp(word, "note") == 0) {
            if (loaded.noteCount >= SONG_MAX_NOTES) break;
            Note n;
            memset(&n, 0, sizeof(n));
            if (fscanf(f, "%d %d %lf %lf %d %d", &n.midi, &n.velocity, &n.rawStartSec, &n.rawEndSec, &n.startTick, &n.lenTick) == 6) {
                n.midi = ClampIntLocal(n.midi, 0, 127);
                n.velocity = ClampIntLocal(n.velocity, 1, 127);
                if (n.lenTick <= 0) {
                    n.startTick = Song_QuantizeTick(&loaded, Song_SecondsToTicks(&loaded, n.rawStartSec));
                    int endTick = Song_QuantizeTick(&loaded, Song_SecondsToTicks(&loaded, n.rawEndSec));
                    if (endTick <= n.startTick) endTick = n.startTick + Song_GridTicks(&loaded);
                    n.lenTick = endTick - n.startTick;
                }
                loaded.notes[loaded.noteCount++] = n;
            }
        }
        else {
            // Unknown token. Stop rather than reading garbage.
            break;
        }
    }

    (void)expectedNotes;
    fclose(f);
    loaded.dirty = false;
    Song_Sort(&loaded);
    *song = loaded;
    return true;
}
