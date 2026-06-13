#ifndef EDITOR_H
#define EDITOR_H

#include "song.h"
#include <stdbool.h>

typedef struct Editor {
    float scrollX;
    int minPitch;
    int maxPitch;
    float pixelsPerBeat;
    char status[256];
    double statusTimer;
    bool showHelp;
} Editor;

void Editor_Init(Editor* editor);
void Editor_SetStatus(Editor* editor, const char* text);
void Editor_Update(Editor* editor, Song* song, float dt);
void Editor_Draw(Editor* editor, const Song* song, bool recording, double recordTime, const char* midiStatus);

#endif
