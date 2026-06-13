#include "lilypond_export.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int GcdInt(int a, int b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

static int NoteCompareExport(const void* a, const void* b)
{
    const Note* na = (const Note*)a;
    const Note* nb = (const Note*)b;
    if (na->startTick != nb->startTick) return na->startTick - nb->startTick;
    if (na->midi != nb->midi) return na->midi - nb->midi;
    return na->lenTick - nb->lenTick;
}

static void DurationToLily(int ticks, char* out, int outSize)
{
    if (!out || outSize <= 0) return;
    if (ticks <= 0) ticks = SONG_PPQ;

    const int whole = SONG_PPQ * 4;
    const int denoms[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
    const int count = (int)(sizeof(denoms) / sizeof(denoms[0]));

    for (int i = 0; i < count; i++) {
        int d = denoms[i];
        int base = whole / d;
        if (base > 0 && ticks == base) {
            snprintf(out, (size_t)outSize, "%d", d);
            return;
        }
    }

    for (int i = 0; i < count; i++) {
        int d = denoms[i];
        int base = whole / d;
        if (base > 0 && ticks * 2 == base * 3) {
            snprintf(out, (size_t)outSize, "%d.", d);
            return;
        }
    }

    for (int i = 0; i < count; i++) {
        int d = denoms[i];
        int base = whole / d;
        if (base > 0 && ticks * 4 == base * 7) {
            snprintf(out, (size_t)outSize, "%d..", d);
            return;
        }
    }

    // Fallback: LilyPond duration multiplier, e.g. c1*3/8.
    int num = ticks;
    int den = whole;
    int g = GcdInt(num, den);
    num /= g;
    den /= g;
    snprintf(out, (size_t)outSize, "1*%d/%d", num, den);
}

static void MidiToLilyPitch(int midi, char* out, int outSize)
{
    static const char* names[12] = {
        "c", "cis", "d", "dis", "e", "f", "fis", "g", "gis", "a", "ais", "b"
    };

    if (!out || outSize <= 0) return;
    if (midi < 0) midi = 0;
    if (midi > 127) midi = 127;

    int pc = midi % 12;
    int scientificOctave = midi / 12 - 1;
    int lilyMarks = scientificOctave - 3; // MIDI 60/C4 becomes c'

    snprintf(out, (size_t)outSize, "%s", names[pc]);
    int len = (int)strlen(out);

    if (lilyMarks > 0) {
        for (int i = 0; i < lilyMarks && len < outSize - 1; i++) out[len++] = '\'';
    }
    else if (lilyMarks < 0) {
        for (int i = 0; i < -lilyMarks && len < outSize - 1; i++) out[len++] = ',';
    }
    out[len] = '\0';
}

static void WriteWrappedToken(FILE* f, const char* token, int* col)
{
    if (!f || !token || !col) return;
    int len = (int)strlen(token);
    if (*col + len + 1 > 96) {
        fprintf(f, "\n    ");
        *col = 4;
    }
    fprintf(f, "%s ", token);
    *col += len + 1;
}

bool Lilypond_Export(const Song* song, const char* path)
{
    if (!song || !path) return false;

    FILE* f = fopen(path, "wb");
    if (!f) return false;

    Note* temp = NULL;
    if (song->noteCount > 0) {
        temp = (Note*)malloc((size_t)song->noteCount * sizeof(Note));
        if (!temp) {
            fclose(f);
            return false;
        }
        memcpy(temp, song->notes, (size_t)song->noteCount * sizeof(Note));
        qsort(temp, (size_t)song->noteCount, sizeof(Note), NoteCompareExport);
    }

    int top = song->timeSigTop > 0 ? song->timeSigTop : 4;
    int bottom = song->timeSigBottom > 0 ? song->timeSigBottom : 4;
    int tempo = (int)(song->bpm + 0.5);

    fprintf(f, "\\version \"2.24.0\"\n");
    fprintf(f, "\\header {\n");
    fprintf(f, "  title = \"MIDI Sheet Raylib Export\"\n");
    fprintf(f, "  composer = \"\"\n");
    fprintf(f, "}\n\n");
    fprintf(f, "%% This is a simple sketch export: notes with the same snapped start are emitted as chords.\n");
    fprintf(f, "%% More advanced overlapping/independent voices can be cleaned up later in a notation editor.\n\n");
    fprintf(f, "\\score {\n");
    fprintf(f, "  \\new Staff \\absolute {\n");
    fprintf(f, "    \\clef treble\n");
    fprintf(f, "    \\time %d/%d\n", top, bottom);
    fprintf(f, "    \\tempo 4 = %d\n", tempo);
    fprintf(f, "    ");

    int col = 4;
    int cursor = 0;
    int barTicks = SONG_PPQ * top * 4 / bottom;
    if (barTicks <= 0) barTicks = SONG_PPQ * 4;

    int i = 0;
    while (i < song->noteCount) {
        int start = temp[i].startTick;
        if (start < 0) start = 0;

        if (start > cursor) {
            char dur[32];
            char tok[64];
            DurationToLily(start - cursor, dur, sizeof(dur));
            snprintf(tok, sizeof(tok), "r%s", dur);
            WriteWrappedToken(f, tok, &col);
            cursor = start;
        }

        int groupStart = start;
        int groupEnd = temp[i].startTick + temp[i].lenTick;
        int j = i + 1;
        while (j < song->noteCount && temp[j].startTick == groupStart) {
            int e = temp[j].startTick + temp[j].lenTick;
            if (e > groupEnd) groupEnd = e;
            j++;
        }

        int groupLen = groupEnd - groupStart;
        if (groupLen <= 0) groupLen = Song_GridTicks(song);

        char dur[32];
        DurationToLily(groupLen, dur, sizeof(dur));

        if (j - i == 1) {
            char pitch[32];
            char tok[96];
            MidiToLilyPitch(temp[i].midi, pitch, sizeof(pitch));
            snprintf(tok, sizeof(tok), "%s%s", pitch, dur);
            WriteWrappedToken(f, tok, &col);
        }
        else {
            char tok[512];
            int pos = 0;
            tok[pos++] = '<';
            for (int k = i; k < j; k++) {
                char pitch[32];
                MidiToLilyPitch(temp[k].midi, pitch, sizeof(pitch));
                int wrote = snprintf(tok + pos, sizeof(tok) - (size_t)pos, "%s%s", (k == i ? "" : " "), pitch);
                if (wrote < 0) break;
                pos += wrote;
                if (pos >= (int)sizeof(tok) - 32) break;
            }
            snprintf(tok + pos, sizeof(tok) - (size_t)pos, ">%s", dur);
            WriteWrappedToken(f, tok, &col);
        }

        cursor = groupEnd;
        if (barTicks > 0 && cursor > 0 && (cursor % barTicks) == 0) {
            WriteWrappedToken(f, "|", &col);
        }

        i = j;
    }

    fprintf(f, "\n  }\n");
    fprintf(f, "  \\layout { }\n");
    fprintf(f, "  \\midi { }\n");
    fprintf(f, "}\n");

    if (temp) free(temp);
    fclose(f);
    return true;
}
