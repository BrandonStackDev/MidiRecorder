MIDI Sheet Raylib
=================

Tiny raylib + Windows MIDI input starter for recording a MIDI keyboard into a simple piano-roll editor and exporting a basic LilyPond file.

What it does now
----------------
- Opens the first Windows MIDI input device through winmm.
- Records MIDI note-on/note-off messages with raw seconds preserved.
- Quantizes the visible/editor notes from BPM + grid.
- Lets you change BPM after recording; notes are re-snapped from the raw recorded timing.
- Supports chords by letting multiple notes share the same snapped start time.
- Lets you click a note and edit pitch, start position, and length.
- Exports out.ly and out.txt with simple LilyPond syntax.
- Saves/loads a tiny project file named song_project.txt.

Build notes
-----------
This is set up like your existing Visual Studio CMake raylib tests:

- CMake uses C:/raylib
- generator: Ninja
- target: midi_sheet
- links: raylib opengl32 gdi32 winmm user32 shell32

To use:
1. Make a new folder/repo.
2. Unzip this project into it.
3. Open the folder in Visual Studio.
4. Configure x64-Debug.
5. Build and run midi_sheet.

MIDI setup
----------
Plug in your MIDI keyboard before launching the app. The starter opens the first MIDI input device it finds. If the keyboard was not plugged in, plug it in and restart the app. You can also press M to retry opening MIDI if it was closed/not found.

Controls
--------
R                 Start/stop recording
E                 Export out.ly and out.txt
M                 Retry MIDI open
G                 Cycle grid: 1/8, 1/16, 1/32, 1/64, 1/128
[ / ]             BPM down/up by 1
Shift + [ / ]     BPM down/up by 10
Click note         Select note
Up / Down          Selected note pitch +/- 1 semitone
Shift + Up/Down    Selected note pitch +/- 1 octave
Left / Right       Selected note start by one grid step
Shift + Left/Right Selected note start by one beat/bar-ish step
, / .              Shrink/grow selected note length by one grid step
Delete/Backspace   Delete selected note
Ctrl+S             Save song_project.txt
Ctrl+L             Load song_project.txt
Ctrl+N             New blank song
Mouse wheel        Horizontal scroll
Ctrl+wheel         Zoom timeline
Shift+wheel        Move visible pitch range
F1                 Show/hide help box

How the BPM/grid jerrymander works
----------------------------------
Each recorded note keeps raw seconds: rawStartSec and rawEndSec.

When BPM or grid changes, the note's startTick and lenTick are rebuilt like this:

    seconds -> beats using BPM -> ticks -> nearest grid bucket

So if you play something freely, you can try BPM 112, 116, 120, etc. and the piano roll will snap differently without losing the original take timing.

LilyPond export notes
---------------------
The export is intentionally simple:
- Notes with the exact same snapped start become one chord like <c' e' g'>4.
- Rests are inserted between non-overlapping events.
- Complex overlapping independent voices are flattened into a simple staff sketch.

That means the .ly is meant as a clean starting point for a notation editor, not final engraving. The next big upgrade would be true voice splitting, left/right hand staff splitting, playback, and maybe MusicXML export.

Files worth editing first
-------------------------
src/main.c              app loop, hotkeys, MIDI recording state
src/song.c/.h           note storage, BPM/grid quantizing, save/load
src/editor.c/.h         piano roll drawing and note editing
src/lilypond_export.c   LilyPond writer
src/midi_input.c/.h     Windows winmm MIDI input

