#include "raylib.h"

#include "editor.h"
#include "lilypond_export.h"
#include "midi_input.h"
#include "song.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define PROJECT_SAVE_PATH "song_project.txt"
#define LILYPOND_EXPORT_PATH "out.ly"
#define LILYPOND_EXPORT_COPY_PATH "out.txt"

typedef struct ActiveMidiNote {
    bool down;
    double rawStartSec;
    int velocity;
} ActiveMidiNote;

static bool IsCtrlDown(void)
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
}

static bool IsShiftDown(void)
{
    return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
}

static void FinishActiveNote(Song* song, ActiveMidiNote active[128], int midi, double rawEndSec)
{
    if (!song || !active) return;
    if (midi < 0 || midi >= 128) return;
    if (!active[midi].down) return;

    Song_AddRawNote(song, midi, active[midi].velocity, active[midi].rawStartSec, rawEndSec);
    active[midi].down = false;
    active[midi].rawStartSec = 0.0;
    active[midi].velocity = 0;
}

static void FinishAllActiveNotes(Song* song, ActiveMidiNote active[128], double rawEndSec)
{
    for (int i = 0; i < 128; i++) {
        if (active[i].down) FinishActiveNote(song, active, i, rawEndSec);
    }
}

static void HandleMidiEvent(Song* song, ActiveMidiNote active[128], MidiEvent ev, bool recording, double recordStartSec)
{
    if (!recording) return;

    unsigned int msg = ev.message;
    int status = (int)(msg & 0xFF);
    int data1 = (int)((msg >> 8) & 0xFF);
    int data2 = (int)((msg >> 16) & 0xFF);

    int type = status & 0xF0;
    int midi = data1;
    int velocity = data2;
    double rawTime = ev.timeSec - recordStartSec;
    if (rawTime < 0.0) rawTime = 0.0;

    bool noteOn = (type == 0x90 && velocity > 0);
    bool noteOff = (type == 0x80) || (type == 0x90 && velocity == 0);

    if (midi < 0 || midi >= 128) return;

    if (noteOn) {
        // If the keyboard repeats a note without sending off first, close the previous one.
        if (active[midi].down) {
            FinishActiveNote(song, active, midi, rawTime);
        }
        active[midi].down = true;
        active[midi].rawStartSec = rawTime;
        active[midi].velocity = velocity;
    }
    else if (noteOff) {
        FinishActiveNote(song, active, midi, rawTime);
    }
}

static void CopyFileSimple(const char* srcPath, const char* dstPath)
{
    FILE* in = fopen(srcPath, "rb");
    if (!in) return;
    FILE* out = fopen(dstPath, "wb");
    if (!out) {
        fclose(in);
        return;
    }

    char buffer[4096];
    size_t n = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        fwrite(buffer, 1, n, out);
    }

    fclose(out);
    fclose(in);
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1360, 820, "MIDI Sheet Raylib");
    SetTargetFPS(60);

    Song song;
    Song_Init(&song);

    Editor editor;
    Editor_Init(&editor);

    MidiInput midi;
    MidiInput_Init(&midi);
    MidiInput_OpenFirst(&midi);

    ActiveMidiNote active[128] = { 0 };
    bool recording = false;
    double recordStartSec = 0.0;
    double recordNowRaw = 0.0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        MidiEvent ev;
        while (MidiInput_PollEvent(&midi, &ev)) {
            HandleMidiEvent(&song, active, ev, recording, recordStartSec);
        }

        if (recording) {
            recordNowRaw = MidiInput_NowSec() - recordStartSec;
            if (recordNowRaw < 0.0) recordNowRaw = 0.0;
        }

        if (IsKeyPressed(KEY_R)) {
            if (!recording) {
                memset(active, 0, sizeof(active));
                recordStartSec = MidiInput_NowSec();
                recordNowRaw = 0.0;
                recording = true;
                Editor_SetStatus(&editor, "Recording started");
            }
            else {
                recordNowRaw = MidiInput_NowSec() - recordStartSec;
                FinishAllActiveNotes(&song, active, recordNowRaw);
                recording = false;
                Song_Sort(&song);
                Editor_SetStatus(&editor, "Recording stopped");
            }
        }

        if (IsKeyPressed(KEY_M)) {
            if (!MidiInput_IsOpen(&midi)) {
                MidiInput_OpenFirst(&midi);
                Editor_SetStatus(&editor, MidiInput_StatusText(&midi));
            }
            else {
                Editor_SetStatus(&editor, "MIDI is already open");
            }
        }

        if (IsKeyPressed(KEY_G)) {
            Song_CycleGrid(&song);
            Editor_SetStatus(&editor, TextFormat("Grid set to 1/%d", song.gridDenom));
        }

        if (IsKeyPressed(KEY_LEFT_BRACKET)) {
            double step = IsShiftDown() ? 10.0 : 1.0;
            Song_SetBpm(&song, song.bpm - step, true);
            Editor_SetStatus(&editor, TextFormat("BPM %.1f - notes re-snapped from raw timing", song.bpm));
        }

        if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
            double step = IsShiftDown() ? 10.0 : 1.0;
            Song_SetBpm(&song, song.bpm + step, true);
            Editor_SetStatus(&editor, TextFormat("BPM %.1f - notes re-snapped from raw timing", song.bpm));
        }

        if (IsKeyPressed(KEY_E)) {
            bool ok = Lilypond_Export(&song, LILYPOND_EXPORT_PATH);
            if (ok) {
                CopyFileSimple(LILYPOND_EXPORT_PATH, LILYPOND_EXPORT_COPY_PATH);
                Editor_SetStatus(&editor, "Exported out.ly and out.txt");
            }
            else {
                Editor_SetStatus(&editor, "Export failed");
            }
        }

        if (IsCtrlDown() && IsKeyPressed(KEY_S)) {
            if (Song_SaveProject(&song, PROJECT_SAVE_PATH)) {
                Editor_SetStatus(&editor, "Saved song_project.txt");
            }
            else {
                Editor_SetStatus(&editor, "Save failed");
            }
        }

        if (IsCtrlDown() && IsKeyPressed(KEY_L)) {
            if (Song_LoadProject(&song, PROJECT_SAVE_PATH)) {
                Editor_SetStatus(&editor, "Loaded song_project.txt");
            }
            else {
                Editor_SetStatus(&editor, "Load failed - no song_project.txt yet?");
            }
        }

        if (IsCtrlDown() && IsKeyPressed(KEY_N)) {
            Song_Clear(&song);
            memset(active, 0, sizeof(active));
            recording = false;
            Editor_SetStatus(&editor, "New blank song");
        }

        Editor_Update(&editor, &song, dt);

        BeginDrawing();
        Editor_Draw(&editor, &song, recording, recordNowRaw, MidiInput_StatusText(&midi));
        EndDrawing();
    }

    if (recording) {
        recordNowRaw = MidiInput_NowSec() - recordStartSec;
        FinishAllActiveNotes(&song, active, recordNowRaw);
    }

    MidiInput_Close(&midi);
    CloseWindow();
    return 0;
}
