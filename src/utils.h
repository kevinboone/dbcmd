/*---------------------------------------------------------------------------
dbcmd
utils.h
GPL v3.0
---------------------------------------------------------------------------*/

#pragma once

#include "bool.h"
#include "list.h"

BOOL utils_contains_wildcards (char *filename);
void utils_expand_path (const char *dir, const char *pattern, List *paths, BOOL recursive);
BOOL utils_is_directory (char *filename);
void utils_make_directory (const char *filename);
char *utils_read_stream_to_end (FILE *f);

