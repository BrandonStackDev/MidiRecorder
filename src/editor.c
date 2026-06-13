#include "editor.h"

#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define EDITOR_LEFT     78
#define EDITOR_TOP      112
#define EDITOR_BOTTOM   34
#define NOTE_H          12
#define MIN_NOTE_W      4.0f

static int ClampIntLocal(int v, int mn, int mx)
{
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

static float ClampFloatLocal(float v, float mn, float mx)
{
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

static bool IsBlackKey(int midi)
{
    int n = midi % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

static const char* NoteName(int midi)
{
    static char buf[16];
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int octave = midi / 12 - 1;
    snprintf(buf, sizeof(buf), "%s%d", names[midi % 12], octave);
    return buf;
}

static int Editor_ViewHeight(const Editor* editor)
{
    (void)editor;
    return GetScreenHeight() - EDITOR_TOP - EDITOR_BOTTOM;
}

static float TickToX(const Editor* editor, int tick)
{
    return (float)EDITOR_LEFT + ((float)tick / (float)SONG_PPQ) * editor->pixelsPerBeat - editor->scrollX;
}

static int XToTick(const Editor* editor, const Song* song, float x)
{
    float local = x - (float)EDITOR_LEFT + editor->scrollX;
    int tick = (int)roundf((local / editor->pixelsPerBeat) * (float)SONG_PPQ);
    return Song_QuantizeTick(song, tick);
}

static float MidiToY(const Editor* editor, int midi)
{
    return (float)EDITOR_TOP + (float)(editor->maxPitch - midi) * (float)NOTE_H;
}

static int YToMidi(const Editor* editor, float y)
{
    int row = (int)floorf((y - (float)EDITOR_TOP) / (float)NOTE_H);
    return editor->maxPitch - row;
}

static int HitTestNote(const Editor* editor, const Song* song, Vector2 mouse)
{
    if (!editor || !song) return -1;

    for (int i = song->noteCount - 1; i >= 0; i--) {
        const Note* n = &song->notes[i];
        if (n->midi < editor->minPitch || n->midi > editor->maxPitch) continue;

        float x = TickToX(editor, n->startTick);
        float w = fmaxf(MIN_NOTE_W, ((float)n->lenTick / (float)SONG_PPQ) * editor->pixelsPerBeat);
        float y = MidiToY(editor, n->midi);
        Rectangle r = { x, y + 1.0f, w, (float)NOTE_H - 2.0f };
        if (CheckCollisionPointRec(mouse, r)) return i;
    }

    return -1;
}

void Editor_Init(Editor* editor)
{
    if (!editor) return;
    memset(editor, 0, sizeof(*editor));
    editor->scrollX = 0.0f;
    editor->minPitch = 36; // C2
    editor->maxPitch = 84; // C6
    editor->pixelsPerBeat = 150.0f;
    editor->showHelp = true;
    snprintf(editor->status, sizeof(editor->status), "Ready");
}

void Editor_SetStatus(Editor* editor, const char* text)
{
    if (!editor) return;
    snprintf(editor->status, sizeof(editor->status), "%s", text ? text : "");
    editor->statusTimer = 4.0;
}

void Editor_Update(Editor* editor, Song* song, float dt)
{
    if (!editor || !song) return;

    if (editor->statusTimer > 0.0) editor->statusTimer -= (double)dt;

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            editor->pixelsPerBeat += wheel * 10.0f;
            editor->pixelsPerBeat = ClampFloatLocal(editor->pixelsPerBeat, 60.0f, 420.0f);
        }
        else if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            editor->minPitch += (int)wheel;
            editor->maxPitch += (int)wheel;
            editor->minPitch = ClampIntLocal(editor->minPitch, 0, 110);
            editor->maxPitch = ClampIntLocal(editor->maxPitch, editor->minPitch + 12, 127);
        }
        else {
            editor->scrollX -= wheel * 72.0f;
            if (editor->scrollX < 0.0f) editor->scrollX = 0.0f;
        }
    }

    if (IsKeyPressed(KEY_F1)) {
        editor->showHelp = !editor->showHelp;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 m = GetMousePosition();
        int hit = HitTestNote(editor, song, m);
        if (hit >= 0) {
            Song_SelectOnly(song, hit);
            char msg[128];
            snprintf(msg, sizeof(msg), "Selected %s", NoteName(song->notes[hit].midi));
            Editor_SetStatus(editor, msg);
        }
        else if (m.x >= EDITOR_LEFT && m.y >= EDITOR_TOP && m.y <= GetScreenHeight() - EDITOR_BOTTOM) {
            Song_DeselectAll(song);
        }
    }

    int selected = Song_SelectedIndex(song);
    int grid = Song_GridTicks(song);
    int pitchStep = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 12 : 1;
    int moveStep = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? grid * 4 : grid;

    if (selected >= 0) {
        if (IsKeyPressed(KEY_UP)) {
            Song_MoveSelectedPitch(song, pitchStep);
        }
        if (IsKeyPressed(KEY_DOWN)) {
            Song_MoveSelectedPitch(song, -pitchStep);
        }
        if (IsKeyPressed(KEY_LEFT)) {
            Song_MoveSelectedStart(song, -moveStep);
        }
        if (IsKeyPressed(KEY_RIGHT)) {
            Song_MoveSelectedStart(song, moveStep);
        }
        if (IsKeyPressed(KEY_COMMA)) {
            Song_ResizeSelected(song, -grid);
        }
        if (IsKeyPressed(KEY_PERIOD)) {
            Song_ResizeSelected(song, grid);
        }
        if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
            Song_RemoveNote(song, selected);
            Editor_SetStatus(editor, "Deleted selected note");
        }
    }

    if (IsKeyDown(KEY_A)) {
        editor->scrollX -= 420.0f * dt;
        if (editor->scrollX < 0.0f) editor->scrollX = 0.0f;
    }
    if (IsKeyDown(KEY_D)) {
        editor->scrollX += 420.0f * dt;
    }
}

static void DrawBarLines(const Editor* editor, const Song* song)
{
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int viewBottom = screenH - EDITOR_BOTTOM;

    int firstTick = XToTick(editor, song, (float)EDITOR_LEFT);
    int lastTick = XToTick(editor, song, (float)screenW);
    int beatTicks = Song_TicksPerBeat(song);
    int barTicks = beatTicks * song->timeSigTop * 4 / song->timeSigBottom;
    if (barTicks <= 0) barTicks = beatTicks * 4;

    int startBeat = firstTick / beatTicks;
    int endBeat = lastTick / beatTicks + 2;

    for (int beat = startBeat; beat <= endBeat; beat++) {
        int tick = beat * beatTicks;
        float x = TickToX(editor, tick);
        bool bar = (tick % barTicks) == 0;
        Color c = bar ? Fade(RAYWHITE, 0.36f) : Fade(RAYWHITE, 0.14f);
        DrawLine((int)x, EDITOR_TOP, (int)x, viewBottom, c);

        if (bar) {
            int barNum = tick / barTicks + 1;
            DrawText(TextFormat("%d", barNum), (int)x + 4, EDITOR_TOP - 18, 14, Fade(RAYWHITE, 0.72f));
        }
    }

    int grid = Song_GridTicks(song);
    if (grid < beatTicks) {
        int firstGrid = (firstTick / grid) * grid;
        for (int tick = firstGrid; tick <= lastTick + grid; tick += grid) {
            float x = TickToX(editor, tick);
            DrawLine((int)x, EDITOR_TOP, (int)x, viewBottom, Fade(RAYWHITE, 0.055f));
        }
    }
}

void Editor_Draw(Editor* editor, const Song* song, bool recording, double recordTime, const char* midiStatus)
{
    if (!editor || !song) return;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int viewBottom = screenH - EDITOR_BOTTOM;

    ClearBackground((Color){ 20, 22, 26, 255 });

    DrawRectangle(0, 0, screenW, 96, (Color){ 31, 35, 42, 255 });
    DrawText("MIDI Sheet Raylib", 18, 14, 24, RAYWHITE);

    Color recColor = recording ? (Color){ 255, 90, 80, 255 } : Fade(RAYWHITE, 0.74f);
    DrawText(TextFormat("REC: %s   time: %.2fs", recording ? "ON" : "off", recordTime), 18, 44, 18, recColor);
    DrawText(TextFormat("BPM: %.1f   grid: 1/%d   notes: %d   snap ticks: %d", song->bpm, song->gridDenom, song->noteCount, Song_GridTicks(song)), 270, 44, 18, RAYWHITE);
    DrawText(TextFormat("MIDI: %s", midiStatus ? midiStatus : "none"), 18, 70, 16, Fade(RAYWHITE, 0.78f));

    if (editor->statusTimer > 0.0) {
        DrawText(editor->status, screenW - 420, 70, 16, (Color){ 150, 255, 170, 255 });
    }

    DrawRectangle(0, EDITOR_TOP, EDITOR_LEFT, viewBottom - EDITOR_TOP, (Color){ 27, 30, 36, 255 });
    DrawRectangle(EDITOR_LEFT, EDITOR_TOP, screenW - EDITOR_LEFT, viewBottom - EDITOR_TOP, (Color){ 16, 18, 22, 255 });

    for (int midi = editor->minPitch; midi <= editor->maxPitch; midi++) {
        float y = MidiToY(editor, midi);
        if (y + NOTE_H < EDITOR_TOP || y > viewBottom) continue;

        bool black = IsBlackKey(midi);
        Color row = black ? (Color){ 22, 24, 30, 255 } : (Color){ 29, 31, 38, 255 };
        if ((midi % 12) == 0) row = (Color){ 35, 39, 47, 255 };

        DrawRectangle(EDITOR_LEFT, (int)y, screenW - EDITOR_LEFT, NOTE_H, row);
        DrawLine(EDITOR_LEFT, (int)y, screenW, (int)y, Fade(BLACK, 0.32f));

        Rectangle key = { 6.0f, y + 1.0f, (float)EDITOR_LEFT - 12.0f, (float)NOTE_H - 2.0f };
        DrawRectangleRec(key, black ? (Color){ 10, 10, 12, 255 } : (Color){ 210, 213, 218, 255 });
        if ((midi % 12) == 0) {
            DrawText(NoteName(midi), 12, (int)y + 1, 10, black ? RAYWHITE : BLACK);
        }
    }

    DrawBarLines(editor, song);

    for (int i = 0; i < song->noteCount; i++) {
        const Note* n = &song->notes[i];
        if (n->midi < editor->minPitch || n->midi > editor->maxPitch) continue;

        float x = TickToX(editor, n->startTick);
        float w = fmaxf(MIN_NOTE_W, ((float)n->lenTick / (float)SONG_PPQ) * editor->pixelsPerBeat);
        float y = MidiToY(editor, n->midi);

        if (x + w < EDITOR_LEFT || x > screenW) continue;

        Color fill = n->selected ? (Color){ 255, 207, 90, 255 } : (Color){ 95, 195, 255, 255 };
        Color edge = n->selected ? (Color){ 255, 255, 230, 255 } : (Color){ 170, 230, 255, 255 };
        Rectangle r = { x, y + 1.0f, w, (float)NOTE_H - 2.0f };
        DrawRectangleRounded(r, 0.20f, 4, fill);
        DrawRectangleLinesEx(r, 1.0f, edge);
    }

    int selected = Song_SelectedIndex(song);
    if (selected >= 0) {
        const Note* n = &song->notes[selected];
        DrawRectangle(8, screenH - 30, 760, 22, Fade(BLACK, 0.35f));
        DrawText(TextFormat("Selected: %s  midi:%d  startTick:%d  lenTick:%d  raw: %.3f-%.3f sec",
            NoteName(n->midi), n->midi, n->startTick, n->lenTick, n->rawStartSec, n->rawEndSec),
            16, screenH - 26, 14, RAYWHITE);
    }

    if (editor->showHelp) {
        int x = screenW - 430;
        int y = 110;
        DrawRectangle(x - 12, y - 10, 420, 238, Fade(BLACK, 0.58f));
        DrawText("Controls", x, y, 20, RAYWHITE); y += 28;
        DrawText("R: record on/off    E: export out.ly/out.txt", x, y, 14, RAYWHITE); y += 20;
        DrawText("[ / ]: BPM down/up  Shift = x10", x, y, 14, RAYWHITE); y += 20;
        DrawText("G: cycle grid 1/8..1/128", x, y, 14, RAYWHITE); y += 20;
        DrawText("Click note: select", x, y, 14, RAYWHITE); y += 20;
        DrawText("Arrows: pitch/start    Shift = octave/bar", x, y, 14, RAYWHITE); y += 20;
        DrawText(", / .: shrink/grow selected length", x, y, 14, RAYWHITE); y += 20;
        DrawText("Delete: remove selected note", x, y, 14, RAYWHITE); y += 20;
        DrawText("Ctrl+S: save project   Ctrl+L: load", x, y, 14, RAYWHITE); y += 20;
        DrawText("Mouse wheel: scroll   Ctrl+wheel: zoom", x, y, 14, RAYWHITE); y += 20;
        DrawText("Shift+wheel: move pitch view", x, y, 14, RAYWHITE); y += 20;
        DrawText("F1: hide/show this help", x, y, 14, Fade(RAYWHITE, 0.75f));
    }
}
