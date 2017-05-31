/*---------------------------------------------------------------------------
dbcmd
dropbox.h
GPL v3.0
---------------------------------------------------------------------------*/

#pragma once

#include "bool.h"
#include "list.h"
#include "dropbox_stat.h"


void dropbox_move (const char *token, const char *old_path, 
           const char *new_path, char **error);
void dropbox_get_usage (const char *token, int64_t *quota, 
           int64_t *usage, char **error);
void dropbox_newfolder (const char *token, const char *new_path,
           char **error);
void dropbox_delete (const char *token, const char *path, 
           char **error);
void  dropbox_download (const char *token, const char *source, 
           const char *target, char **error);
void  dropbox_list_files (const char *token, const char *path, 
           List *list, BOOL include_dirs, BOOL recursive, char **error);
char *dropbox_get_token (const char *code, char **error);
void  dropbox_get_file_info (const char *token, const char *file, 
          DBStat *stat, char **error);
BOOL  dropbox_hash (const char *filename, char output_hash[65], char **error);
void  dropbox_upload (const char *token, const char *source, 
          const char *target, char **error);


