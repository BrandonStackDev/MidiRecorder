#ifndef LILYPOND_EXPORT_H
#define LILYPOND_EXPORT_H

#include "song.h"
#include <stdbool.h>

bool Lilypond_Export(const Song* song, const char* path);

#endif
